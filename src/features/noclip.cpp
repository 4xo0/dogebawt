// noclip.cpp - "Auto NoClip (walk into walls)".
//
// Faithful port of the original's noclip machinery:
//   * collision detour     sub_180085460  (GA+0xA47AD0, qword_1801B2800)
//   * engage/patch          DB_PatchGA_A31A1C / DB_InputThread (GA+0xA31F1C)
//   * disengage watchdog    sub_18002D010
//   * tile query            DB_TileQuery   (cheat-side sub_1800C9AF0)
//
// Mechanism: the game's per-tick move/collision-resolve function (GA+0xA47AD0)
// is detoured. When the gate (word_1801B3240) is up the original is *not*
// called, so the player's position is never clamped against walls -> noclip.
// In addition a 4-byte instruction at GA+0xA31F1C is patched (0x405E0C0F on /
// 0x415E0C0F off), and the per-frame update (GA+0xDA6FF0) is suppressed while
// engaged (handled in il2cpp.cpp via GateActive()).
//
// "Auto" part: each tick the detour tile-queries the player's current cell;
// if it's a blocking/damaging tile (byte_1801B3242 in the original) noclip
// engages, and disengages 250 ms after the player clears the walls.
#include "features.h"
#include "config.h"
#include "il2cpp.h"
#include "util.h"
#include "log.h"

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include "MinHook.h"

namespace noclip {
namespace {

// GA+0xA31F1C dword: engaged value written by DB_PatchGA_A31A1C / DB_InputThread
// (1079948815). The original restores 0x415E0C0F (1096726031); we capture the
// live bytes on first engage and restore those, with that as the fallback.
// GA+0xA31F1C is `movzx ebx, [rsi+0x41]` (0F B6 5E 41). Noclip flips the
// displacement to +0x40 (0F B6 5E 40) so the collision check reads the wrong
// (non-solid) flag byte. Values are the exact dwords from DB_PatchGA_A31A1C /
// sub_18002D010 (verified: 1079948815 / 1096726031). A wrong value here writes a
// bogus instruction over live code and crashes the game.
constexpr uint32_t kPatchEngage = 0x405EB60Fu;         // == 1079948815  (read [rsi+0x40])
constexpr uint32_t kPatchRestoreDefault = 0x415EB60Fu; // == 1096726031  (read [rsi+0x41])
constexpr ULONGLONG kOffWallGraceMs = 250; // DB_InputThread's 250ms disengage delay

using CollisionFn = void(__fastcall*)(uintptr_t, uint32_t);
CollisionFn g_orig = nullptr;

std::atomic<bool> g_gate{false};   // word_1801B3240: block collision + per-frame update
bool      g_engaged = false;       // patch currently applied
uint32_t  g_originalDword = kPatchRestoreDefault;
bool      g_haveOriginal = false;
bool      g_onWall = false;        // byte_1801B3242: set by the detour, consumed by Poll
ULONGLONG g_offWallSince = 0;

// Diagnostic snapshot of the last tile the player was queried on.
int     g_dbgType = -1;
uint8_t g_dbgObj1698 = 0;
uint8_t g_dbgB27 = 0;
bool    g_dbgB25 = false;
int     g_dbgDmg = 0;
bool    g_dbgHadTile = false;
bool    g_manual = false;  // held engaged by SocketFu (not the auto wall logic)

// ---------------------------------------------------------------------------
// DB_TileQuery (sub_1800C9AF0) reimplementation - returns whether the cell at
// world (x, y) is a wall/blocking or damaging tile (the original's tile[27] ||
// tile[25] test in sub_180085460).
// ---------------------------------------------------------------------------
bool TileIsWall(float x, float y) {
    if (x < 0.0f || y < 0.0f)
        return false;

    uintptr_t world = game::Root();
    if (!world)
        return false;
    uintptr_t ws = *reinterpret_cast<uintptr_t*>(world + 40);
    if (!ws)
        return false;

    uintptr_t tiles = *reinterpret_cast<uintptr_t*>(ws + 88);
    if (!tiles)
        return false;

    const int height = *reinterpret_cast<int*>(ws + 256); // y-bound / stride
    const int width  = *reinterpret_cast<int*>(ws + 252); // x-bound
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    if (iy >= height || ix >= width)
        return false;

    // tile = *(qword*)(8*(x + height*y) + tiles + 32)
    uintptr_t tile = *reinterpret_cast<uintptr_t*>(
        static_cast<uintptr_t>(8 * (ix + height * iy)) + tiles + 32);
    if (!tile)
        return true; // no tile object -> treated as blocked (result[24]=1)

    g_dbgHadTile = true;
    const uintptr_t obj  = *reinterpret_cast<uintptr_t*>(tile + 72);
    const int       type = *reinterpret_cast<int*>(tile + 68);

    bool b25 = false;      // result[25]
    int  dmg = 0;          // result[28]
    uint8_t b27 = 0;       // result[27] - tile collision byte
    uint8_t b34 = 0;       // result[34]
    uint8_t obj1698 = 0;   // result[24] from occupant (v16[1698])
    bool haveObj = false;  // v14

    if (obj) {
        uintptr_t inner = *reinterpret_cast<uintptr_t*>(obj + 24);
        if (inner) {
            obj1698 = *reinterpret_cast<uint8_t*>(inner + 1698);
            b34 = *reinterpret_cast<uint8_t*>(inner + 1764);
            haveObj = true;
        }
    }

    const uintptr_t props = *reinterpret_cast<uintptr_t*>(tile + 80);
    if (props) {
        b27 = *reinterpret_cast<uint8_t*>(props + 260);
        dmg = *reinterpret_cast<int*>(props + 268);
    }

    // b25: damaging tile that is type 37 or an occupied non-passable tile.
    if (dmg > 0) {
        if (type == 37 || (haveObj && !b34))
            b25 = true;
    }

    g_dbgType = type; g_dbgObj1698 = obj1698; g_dbgB27 = b27;
    g_dbgB25 = b25; g_dbgDmg = dmg;

    // Faithful to sub_180085460: byte_1801B3242 = (result[27] || result[25]).
    return (b27 != 0) || b25;
}

void Engage() {
    if (g_engaged)
        return;
    void* site = ga::Rva(ga::rva::PATCH_LAGPORT); // GA+0xA31F1C
    if (site) {
        if (!g_haveOriginal) {
            std::memcpy(&g_originalDword, site, sizeof(g_originalDword));
            g_haveOriginal = true;
        }
        uint32_t v = kPatchEngage;
        util::Patch(site, &v, sizeof(v));
    }
    g_gate.store(true, std::memory_order_release);
    g_engaged = true;
    DBLOG("noclip: ENGAGE (patch %p)", site);
}

void Disengage() {
    if (!g_engaged)
        return;
    void* site = ga::Rva(ga::rva::PATCH_LAGPORT);
    if (site) {
        uint32_t v = g_haveOriginal ? g_originalDword : kPatchRestoreDefault;
        util::Patch(site, &v, sizeof(v));
    }
    g_gate.store(false, std::memory_order_release);
    g_engaged = false;
    g_offWallSince = 0;
    DBLOG("noclip: DISENGAGE");
}

// Collision/move-resolve detour (sub_180085460). Detects whether the player is
// standing on a wall, and skips the original resolver while the gate is up.
void __fastcall hkCollision(uintptr_t a1, uint32_t a2) {
    static bool once = false;
    if (!once) { once = true; DBLOG("hkCollision: first call"); }

    // Detection is driven by the move target (NoteMoveTarget), not by the
    // resolved player tile - the player's own cell is always clamped to floor,
    // so it can never reveal a wall being walked into. Here we only enforce the
    // gate: when noclip is engaged, skip the original collision resolver so the
    // player isn't pushed back out of the wall.
    if (!g_gate.load(std::memory_order_acquire)) {
        if (g_orig)
            g_orig(a1, a2);
    }
}

} // namespace

// Called from the move hook (HookMoveUpdate) with the player's requested
// destination - the pre-collision tile they are trying to step onto. This is
// where a wall actually shows up (the resolved player position never does).
void NoteMoveTarget(float x, float y) {
    if (!g_cfg.autoNoClip)
        return;
    const bool wall = TileIsWall(x, y);
    if (wall)
        g_onWall = true;

    static ULONGLONG lastLog = 0;
    ULONGLONG now = GetTickCount64();
    if (now - lastLog > 1000) {
        lastLog = now;
        DBLOG("noclip target=(%.2f,%.2f) wall=%d gate=%d eng=%d | tile=%d type=%d obj1698=%d b27=%d b25=%d dmg=%d",
              x, y, (int)wall, (int)g_gate.load(std::memory_order_acquire),
              (int)g_engaged, (int)g_dbgHadTile, g_dbgType, (int)g_dbgObj1698,
              (int)g_dbgB27, (int)g_dbgB25, g_dbgDmg);
    }
}

bool GateActive() {
    return g_gate.load(std::memory_order_acquire);
}

// Manual hold (SocketFu): engage/keep the noclip gate regardless of the auto
// wall logic. Poll() yields to this so the two don't fight over the gate.
void SetManual(bool on) {
    if (on == g_manual)
        return;
    g_manual = on;
    if (on)
        Engage();
    else
        Disengage();
}

void Install() {
    void* target = ga::Rva(ga::rva::COLLISION_FN);
    DBLOG("noclip::Install: collision target=%p (GA+0x%llX)", target,
          (unsigned long long)ga::rva::COLLISION_FN);
    if (!target)
        return;
    const MH_STATUS st = MH_CreateHook(target, reinterpret_cast<void*>(&hkCollision),
                                       reinterpret_cast<void**>(&g_orig));
    DBLOG("noclip::Install: MH_CreateHook=%d orig=%p", (int)st, (void*)g_orig);
}

void Tick() {
    // No overlay in the original; nothing to draw on the render thread.
}

// Game thread (features::GameTick). Mirrors the auto-noclip arm/disarm block at
// the tail of DB_InputThread: engage when on a wall, disengage 250ms after
// clearing. g_onWall is set by the collision detour and reset here each tick.
void Poll() {
    if (g_manual) {        // SocketFu owns the gate; leave it engaged
        g_onWall = false;
        return;
    }
    if (!g_cfg.autoNoClip) {
        if (g_engaged)
            Disengage();
        g_onWall = false;
        return;
    }

    const ULONGLONG now = GetTickCount64();
    if (g_onWall) {
        if (!g_engaged)
            Engage();
        g_offWallSince = 0;
    } else if (g_engaged) {
        if (g_offWallSince == 0)
            g_offWallSince = now + kOffWallGraceMs;
        else if (now >= g_offWallSince)
            Disengage();
    }
    g_onWall = false;
}

} // namespace noclip
