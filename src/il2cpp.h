// il2cpp.h - GameAssembly (Unity il2cpp) binding layer.
//
// DogeBawt does NOT use il2cpp_* reflection APIs heavily; it mostly detours and
// byte-patches GameAssembly.dll at fixed RVAs and walks object fields by offset.
// All RVAs below are GameAssembly.dll-relative and BUILD-SPECIFIC (re-verify
// after a game update). Source: the original's hook installer sub_180087B90.
#pragma once
#include <cstdint>

namespace ga {
    // Base of GameAssembly.dll (the il2cpp game image).
    uintptr_t Base();
    bool Init();   // resolve Base() once GameAssembly is loaded
    bool Ready();
    void Reset();

    inline void* Rva(uintptr_t rva) {
        uintptr_t b = Base();
        return b ? reinterpret_cast<void*>(b + rva) : nullptr;
    }

    // -- GameAssembly RVAs (from sub_180087B90; offsets given in decimal there) --
    namespace rva {
        constexpr uintptr_t AIM_FN          = 0xA3C610; // +10733072  aimbot detour target (g_orig_aimFn)
        constexpr uintptr_t MOVETO          = 0xA48C10; // +10783760  MoveTo(player, Vector2*) (g_fn_moveTo)
        constexpr uintptr_t MOVE_FN         = 0x1ADF7A0; // dodge detour (realmsense notes)
        constexpr uintptr_t MOVE_SPEED_GET  = 0x1AE5020; // speedhack clamp
        constexpr uintptr_t PATCH_LAGPORT   = 0xA31F1C;  // +10690332  movement-collision patch site (noclip/lag-port; DB_PatchGA_A31A1C)
        constexpr uintptr_t WORLD_ROOT_FN   = 0x11B5560; // +18568544  per-frame World method; its 'this' == root (DB_CaptureRoot)
        constexpr uintptr_t PATCH_AIM       = 0xA4D9B1;  // shoot-angle patch site (DB_SetAutoAimPatch, from disasm)
        constexpr uintptr_t WORLD_UPDATE_FN = 0xDA6830;  // +14313520  per-frame game-thread update (sub_1800856F0 detours this; nexus/lag-port fire here)
        constexpr uintptr_t COLLISION_FN    = 0xA47A90;  // +10779280  move/collision-resolve (sub_180085460 detours this; blocked for noclip)
    }

    // Player/object field offsets (from the record builder sub_1800C9090).
    namespace off {
        constexpr int OBJ_X          = 0x3C;   // float
        constexpr int OBJ_Y          = 0x40;   // float
        constexpr int OBJ_HP         = 0x20C;  // int  (record+16)
        constexpr int OBJ_STATUS_PTR = 0x18;   // ptr; (*+1745)!=0 => targetable
        constexpr int OBJ_INVULN     = 0x215;  // byte (record+26)
        constexpr int OBJ_CONDITIONS = 0x270;  // qword bitmask (record+32)
        constexpr int OBJ_TYPE       = 0x30;   // int (record+56)
        constexpr int STATUS_TARGETABLE = 1745;
    }
}

namespace game {
    // Captured world/manager root (qword_1801B2E10). Set when a hooked game
    // method first runs; see Capture()/Root().
    uintptr_t Root();
    void      CaptureRoot(uintptr_t world);
    void      ResetRuntimeState();

    uintptr_t Player();   // Root()->[+40]->[+72], or 0

    // MoveTo(player, &{x,y}) - issues a move/teleport to a world coordinate.
    void MoveTo(uintptr_t player, float x, float y);

    // Install the root-capture detour on GA+WORLD_ROOT_FN (sub_180085800).
    // Creates a MinHook hook (enabled by the shared MH_EnableHook(ALL)).
    void InstallRootCapture();
    bool RootCaptureInstalled();

    // Install the per-frame game-thread update detour on GA+WORLD_UPDATE_FN.
    // This mirrors sub_1800856F0: it fires features::GameTick() on the game
    // thread (where il2cpp managed calls are safe) before forwarding to the
    // original. Lets nexus issue its GA request without crashing the render
    // thread. When noclip's gate is engaged, the original is skipped (faithful
    // to sub_1800856F0's `!word_1801B3240` guard).
    void InstallGameTick();
    bool GameTickInstalled();
}
