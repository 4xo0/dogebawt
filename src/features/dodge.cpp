// dodge.cpp - Auto Dodge.
//
// Live-projectile path recovered from:
//   sub_180085830 -> sub_18002A0C0 -> sub_1800D3270
//   projectile builder sub_1800C9CA0
//   collision test sub_1800D2AC0
//   ring sampler sub_1800D2E40
//   map/segment test sub_1800D2510
#include "features.h"
#include "config.h"
#include "il2cpp.h"
#include "log.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include "MinHook.h"

namespace dodge {
namespace {

struct Vec2 {
    float x;
    float y;
};

struct PathPoint {
    Vec2 position;
    int timeMs;
};

struct Threat {
    uintptr_t object;
    int startTime;
    int attacker;
    int objectType;
    float lifetimeMs;
    bool visible;
    std::vector<PathPoint> path;
};

struct Candidate {
    Vec2 position{};
    unsigned safeForMs = 0;
    bool legal = false;
};

struct TileInfo {
    bool present = false;
    int type = 37;
    bool blocked = false;
};

// DB_InstallHooks: GA + 10,740,848 and GA + 10,767,104.
constexpr uintptr_t kMoveUpdateRva = 0xA3E470;
constexpr uintptr_t kMoveSpeedRva = 0xA44B00;
// Late assignment at DB_InstallHooks+0x2E0; called by sub_1800C9CA0.
constexpr uintptr_t kProjectilePositionRva = 0x7282F0;
// Player.Update(now, delta) - obf "GJFKGLJEGKO" on player_class. Same game
// build as realmsense (its input_update fallback GA+0x11B5560 == our root fn),
// so this fallback RVA is valid here too.
constexpr uintptr_t kPlayerUpdateRva = 0xA45AD0;

// dword_1801B2CCC = 0x3EE978B9; dword_1801B2CD0 = 200; dword_1801B2CDC = 0x3CA3D70A;
// dword_1801B2CE0 = 0x42740000. All read straight from the binary.
constexpr float kHitboxHalf = 0.455999911f;   // 0x3EE978B9
constexpr int kMoveAwayBufferMs = 200;
constexpr float kCandidatePadding = 0.06f;
constexpr float kDangerWeight = 0.02f;         // 0x3CA3D70A
constexpr float kDistanceWeight = 61.0f;       // 0x42740000
constexpr float kTwoPi = 6.2831855f;
constexpr unsigned kNoThreatTime = 40000;

using MoveUpdateFn = int64_t(__fastcall*)(uintptr_t, float, float);
// The move-speed getter (GA+0xA44B00) is an il2cpp instance method: it reads
// `this` (the player) from rcx. sub_18002A0C0 calls it with rcx still holding
// the player, so it MUST be invoked with the player pointer.
using MoveSpeedFn = float(__fastcall*)(uintptr_t);
using ProjectilePositionFn =
    uint64_t(__fastcall*)(uintptr_t, float, float*, int*);

MoveUpdateFn g_originalMoveUpdate = nullptr;

float Distance(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool QueryTile(Vec2 point, TileInfo& out) {
    out = {};
    if (point.x < 0.0f || point.y < 0.0f) return false;

    const uintptr_t root = game::Root();
    const uintptr_t world = root ? *reinterpret_cast<uintptr_t*>(root + 40) : 0;
    if (!world) return false;
    const uintptr_t squares = *reinterpret_cast<uintptr_t*>(world + 88);
    const int height = *reinterpret_cast<int*>(world + 256);
    const int width = *reinterpret_cast<int*>(world + 252);
    if (!squares || height <= 0 || width <= 0) return false;

    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const uintptr_t square =
        *reinterpret_cast<uintptr_t*>(squares + 32 + 8ull * (x + height * y));
    if (!square) {
        out.blocked = true;
        return true;
    }

    out.present = true;
    out.type = *reinterpret_cast<int*>(square + 68);
    bool fullOccupy = false;
    bool objectAllowsWalk = true;
    const uintptr_t object = *reinterpret_cast<uintptr_t*>(square + 72);
    if (object) {
        const uintptr_t status = *reinterpret_cast<uintptr_t*>(object + 24);
        if (status) {
            fullOccupy = *reinterpret_cast<uint8_t*>(status + 1698) != 0;
            objectAllowsWalk = *reinterpret_cast<uint8_t*>(status + 1764) != 0;
        }
    }

    bool damaging = false;
    int damage = 0;
    const uintptr_t props = *reinterpret_cast<uintptr_t*>(square + 80);
    if (props) {
        damaging = *reinterpret_cast<uint8_t*>(props + 260) != 0;
        damage = *reinterpret_cast<int*>(props + 268);
    }
    out.blocked = out.type == 34 || out.type == 5 || damaging || fullOccupy;
    if (damage > 0 && (out.type == 37 || (object && !objectAllowsWalk)))
        out.blocked = true;
    return true;
}

bool SegmentBlocked(Vec2 from, Vec2 to) {
    // sub_1800D2510 rounds the segment midpoint, tests the four surrounding
    // tile centers, and rejects a blocking center within 0.9 world units.
    const float midpointX = std::round((from.x + to.x) * 0.5f);
    const float midpointY = std::round((from.y + to.y) * 0.5f);
    static constexpr Vec2 corners[] = {
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {-0.5f, 0.5f}, {0.5f, 0.5f},
    };

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float lengthSq = dx * dx + dy * dy;
    for (const Vec2 corner : corners) {
        const Vec2 center{midpointX + corner.x, midpointY + corner.y};
        TileInfo tile;
        if (!QueryTile(center, tile) || !tile.blocked) continue;

        float closestX = to.x;
        float closestY = to.y;
        if (lengthSq == 0.0f) {
            closestX = from.x;
            closestY = from.y;
        } else {
            const float t = ((center.x - from.x) * dx + (center.y - from.y) * dy) /
                            lengthSq;
            if (t < 0.0f) {
                closestX = from.x;
                closestY = from.y;
            } else if (t <= 1.0f) {
                closestX = from.x + t * dx;
                closestY = from.y + t * dy;
            }
        }
        const float ex = center.x - closestX;
        const float ey = center.y - closestY;
        if (std::sqrt(ex * ex + ey * ey) <= 0.9f) return true;
    }
    return false;
}

int CurrentGameTime(uintptr_t world) {
    return world ? *reinterpret_cast<int*>(world + 128) : 0;
}

bool ShooterVisible(int attacker) {
    if (g_cfg.dodgeInvisible) return true;
    const uintptr_t root = game::Root();
    const uintptr_t world = root ? *reinterpret_cast<uintptr_t*>(root + 40) : 0;
    const uintptr_t manager = world ? *reinterpret_cast<uintptr_t*>(world + 176) : 0;
    const uintptr_t list = manager ? *reinterpret_cast<uintptr_t*>(manager + 24) : 0;
    if (!list) return true;
    const uint32_t count = *reinterpret_cast<uint32_t*>(list + 24);
    if (count >= 20000) return true;
    for (uint32_t i = 0; i < count; ++i) {
        const uintptr_t object = *reinterpret_cast<uintptr_t*>(list + 48 + 24ull * i);
        if (!object || *reinterpret_cast<int*>(object + 0x34) != attacker) continue;
        const uintptr_t status = *reinterpret_cast<uintptr_t*>(object + 0x18);
        return status && *reinterpret_cast<uint8_t*>(status + 1745) != 0;
    }
    // The original record starts true and only replaces it on an owner match.
    return true;
}

bool ProjectilePosition(uintptr_t projectile, float futureMs, Vec2& out) {
    auto fn = reinterpret_cast<ProjectilePositionFn>(ga::Rva(kProjectilePositionRva));
    if (!fn) return false;
    // The native positionAtTime writes its out-params through r8/r9. The
    // original gives r8 a 12-byte buffer (_DWORD[3]) and r9 a 4-byte int; a
    // 4-byte r8 here overflows the stack. Match the original's buffer sizes.
    float scratch[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int scratchInt[2] = {0, 0};
    const uint64_t packed = fn(projectile, futureMs, scratch, scratchInt);
    std::memcpy(&out, &packed, sizeof(out));
    return std::isfinite(out.x) && std::isfinite(out.y) &&
           !(out.x == -1.0f && out.y == -1.0f);
}

void GatherThreats(std::vector<Threat>& threats, int gameTime) {
    threats.clear();
    const uintptr_t root = game::Root();
    const uintptr_t world = root ? *reinterpret_cast<uintptr_t*>(root + 40) : 0;
    const uintptr_t manager = world ? *reinterpret_cast<uintptr_t*>(world + 184) : 0;
    const uintptr_t list = manager ? *reinterpret_cast<uintptr_t*>(manager + 24) : 0;
    if (!list) return;

    const uint32_t count = *reinterpret_cast<uint32_t*>(list + 24);
    if (!count || count >= 10000) return;
    threats.reserve(std::min<uint32_t>(count, 256));

    for (uint32_t index = 0; index < count; ++index) {
        const uintptr_t projectile =
            *reinterpret_cast<uintptr_t*>(list + 48 + 24ull * index);
        if (!projectile) continue;

        Threat threat{};
        threat.object = projectile;
        threat.startTime = *reinterpret_cast<int*>(projectile + 364);
        threat.attacker = *reinterpret_cast<int*>(projectile + 368);
        threat.lifetimeMs = *reinterpret_cast<float*>(projectile + 400);
        threat.visible = ShooterVisible(threat.attacker);
        const uintptr_t klass = *reinterpret_cast<uintptr_t*>(projectile + 24);
        threat.objectType = klass ? *reinterpret_cast<int*>(klass + 1700) : 0;

        if (!std::isfinite(threat.lifetimeMs) || threat.lifetimeMs <= 0.0f ||
            threat.lifetimeMs > 125000.0f || threat.startTime <= 0 ||
            !threat.visible)
            continue;
        if (threat.objectType != 14174 &&
            static_cast<float>(gameTime) > threat.startTime + threat.lifetimeMs)
            continue;

        int samples = static_cast<int>(threat.lifetimeMs);
        samples = std::clamp(samples, 1, 300);
        const float interval = threat.lifetimeMs / static_cast<float>(samples);
        threat.path.reserve(samples + 1);
        for (int i = 0; i < samples; ++i) {
            Vec2 position{};
            const float future = static_cast<float>(i) * interval;
            if (!ProjectilePosition(projectile, future, position)) break;
            threat.path.push_back({position, static_cast<int>(future)});
        }
        Vec2 finalPosition{};
        if (ProjectilePosition(projectile, threat.lifetimeMs, finalPosition))
            threat.path.push_back({finalPosition, static_cast<int>(threat.lifetimeMs)});
        if (!threat.path.empty()) threats.push_back(std::move(threat));
    }
}

bool PositionSafe(Vec2 position, int gameTime, const std::vector<Threat>& threats,
                  unsigned& safeForMs) {
    safeForMs = kNoThreatTime;
    if (threats.empty()) return true;

    const float hitboxSq = kHitboxHalf * kHitboxHalf;
    bool safe = true;
    for (const Threat& threat : threats) {
        if (threat.path.empty()) continue;

        size_t first = 0;
        while (first + 1 < threat.path.size() &&
               static_cast<unsigned>(threat.startTime + threat.path[first].timeMs) <
                   static_cast<unsigned>(gameTime))
            ++first;
        if (first > 1) --first;

        bool nearPrevious = false;
        for (size_t i = first; i < threat.path.size(); ++i) {
            const PathPoint& point = threat.path[i];
            const float dx = point.position.x - position.x;
            const float dy = point.position.y - position.y;

            if (i && nearPrevious) {
                const PathPoint& previous = threat.path[i - 1];
                const unsigned span =
                    static_cast<unsigned>(point.timeMs - previous.timeMs);
                if (span >= 4) {
                    for (unsigned step = 1; step < span; step += 2) {
                        const float t = static_cast<float>(step) /
                                        static_cast<float>(span);
                        const float ix = previous.position.x +
                                         (point.position.x - previous.position.x) * t;
                        const float iy = previous.position.y +
                                         (point.position.y - previous.position.y) * t;
                        if ((ix - position.x) * (ix - position.x) < hitboxSq &&
                            (iy - position.y) * (iy - position.y) < hitboxSq) {
                            const unsigned hitTime =
                                static_cast<unsigned>(threat.startTime +
                                                      previous.timeMs + step);
                            if (hitTime >= static_cast<unsigned>(gameTime)) {
                                safeForMs = std::min(
                                    safeForMs, hitTime - static_cast<unsigned>(gameTime));
                                safe = false;
                            }
                            break;
                        }
                    }
                }
            }

            if (dx * dx < hitboxSq && dy * dy < hitboxSq) {
                const unsigned hitTime =
                    static_cast<unsigned>(threat.startTime + point.timeMs);
                if (hitTime >= static_cast<unsigned>(gameTime)) {
                    safeForMs =
                        std::min(safeForMs, hitTime - static_cast<unsigned>(gameTime));
                    safe = false;
                    break;
                }
            }
            nearPrevious = dx * dx < 49.0f && dy * dy < 49.0f;
        }
    }
    return safe;
}

// One ring (sub_1800D2E40): all directional candidates plus the smoothing-best
// (centre of the widest safe arc via the +-3 angular window sum).
struct Ring {
    std::vector<Candidate> candidates;
    Candidate best{};
};

Ring BuildRing(Vec2 center, float scale, float density, int gameTime,
               const std::vector<Threat>& threats) {
    const int directions = std::max(
        1, static_cast<int>(static_cast<int>(scale * 16.0f) * density));
    const float radius = (kHitboxHalf + kCandidatePadding) * scale;

    Ring ring;
    ring.candidates.reserve(directions);
    for (int i = 0; i < directions; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) /
                            static_cast<float>(directions);
        Candidate candidate{};
        candidate.position = {
            center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius,
        };
        candidate.legal = !SegmentBlocked(center, candidate.position);
        if (candidate.legal)
            PositionSafe(candidate.position, gameTime, threats, candidate.safeForMs);
        else
            candidate.safeForMs = 0; // blocked rays count as zero safe time
        ring.candidates.push_back(candidate);
    }

    int64_t bestSmoothed = std::numeric_limits<int64_t>::min();
    for (int i = 0; i < directions; ++i) {
        int64_t smoothed = ring.candidates[i].safeForMs;
        for (int gap = 1; gap <= 3; ++gap) {
            smoothed += ring.candidates[(i + gap) % directions].safeForMs;
            smoothed += ring.candidates[(i - gap + directions) % directions].safeForMs;
        }
        if (smoothed > bestSmoothed) {
            bestSmoothed = smoothed;
            ring.best = ring.candidates[i];
        }
    }
    return ring;
}

// DB_DodgeCore: ring0 (x0.5, dense), ring1..3 (x1/1.5/2). Pick the outer band
// (among 1..3) with the largest smoothed safe time as the attractor direction,
// then commit the candidate from the INNERMOST band (ring0) that best trades off
// closeness to that attractor against safe time.
Candidate ChooseCandidate(Vec2 center, int gameTime,
                          const std::vector<Threat>& threats) {
    const Ring rings[] = {
        BuildRing(center, 0.5f, 2.0f, gameTime, threats),
        BuildRing(center, 1.0f, 1.0f, gameTime, threats),
        BuildRing(center, 1.5f, 1.0f, gameTime, threats),
        BuildRing(center, 2.0f, 1.0f, gameTime, threats),
    };

    int outer = 1;
    if (rings[2].best.safeForMs > rings[outer].best.safeForMs) outer = 2;
    if (rings[3].best.safeForMs > rings[outer].best.safeForMs) outer = 3;
    const Vec2 attractor = rings[outer].best.position;

    Candidate best{};
    double bestScore = -std::numeric_limits<double>::infinity();
    for (const Candidate& candidate : rings[0].candidates) {
        const float dx = candidate.position.x - attractor.x;
        const float dy = candidate.position.y - attractor.y;
        const float distance = std::max(std::sqrt(dx * dx + dy * dy), 1e-6f);
        const double score = static_cast<double>(kDistanceWeight) / distance +
                             static_cast<double>(candidate.safeForMs) * kDangerWeight;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

float MoveSpeed(uintptr_t player) {
    auto fn = reinterpret_cast<MoveSpeedFn>(ga::Rva(kMoveSpeedRva));
    const float speed = fn ? fn(player) : 0.0f;
    return std::isfinite(speed) && speed > 0.0f ? speed : 0.001f;
}

// Faithful port of the dodge core sub_1800D3270. It edits the immediate move
// target in place; the return-anchor / manual-steer pacing lives in the caller
// sub_18002A0C0 and is intentionally not reproduced here (it is Sleep-based and
// only governs idle auto-dodge re-centering, not the dodge decision itself).
int64_t __fastcall HookMoveUpdate(uintptr_t player, float targetX, float targetY) {
    if (!g_cfg.dodgeProjectiles || !player)
        return g_originalMoveUpdate ? g_originalMoveUpdate(player, targetX, targetY) : 0;

    const uintptr_t root = game::Root();
    const uintptr_t world = root ? *reinterpret_cast<uintptr_t*>(root + 40) : 0;
    if (!world)
        return g_originalMoveUpdate ? g_originalMoveUpdate(player, targetX, targetY) : 0;

    static int previousGameTime = 0;
    const int gameTime = CurrentGameTime(world);
    const int frameMs = previousGameTime
        ? std::clamp(gameTime - previousGameTime, 1, 100)
        : 1;
    previousGameTime = gameTime;
    const Vec2 current{
        *reinterpret_cast<float*>(player + 0x3C),
        *reinterpret_cast<float*>(player + 0x40),
    };
    const Vec2 requested{targetX, -targetY}; // the move hook's second axis is inverted

    std::vector<Threat> threats;
    GatherThreats(threats, gameTime);

    // Danger gates (sub_1800D2AC0): time-to-hit for the current cell and for the
    // requested destination.
    unsigned currentSafeFor = kNoThreatTime;
    const bool currentSafe = PositionSafe(current, gameTime, threats, currentSafeFor);
    unsigned requestedSafeFor = kNoThreatTime;
    const bool requestedSafe = PositionSafe(requested, gameTime, threats, requestedSafeFor);

    // Requested destination is safe -> let the move through untouched.
    if (requestedSafe)
        return g_originalMoveUpdate ? g_originalMoveUpdate(player, targetX, targetY) : 0;

    const float speed = MoveSpeed(player);

    if (currentSafe) {
        // Current cell is safe but the requested one is not: cancel the move if
        // the requested cell gets hit before the player could cross half a
        // hitbox; otherwise allow it (the move only grazes the danger).
        Vec2 output = requested;
        if (kHitboxHalf / speed > static_cast<float>(requestedSafeFor))
            output = current;
        return g_originalMoveUpdate ? g_originalMoveUpdate(player, output.x, -output.y) : 0;
    }

    // Current cell is unsafe -> ring-sample an escape and step toward it.
    const Candidate chosen = ChooseCandidate(current, gameTime, threats);
    Vec2 output = requested;
    if (chosen.position.x != 0.0f || chosen.position.y != 0.0f) {
        const float distance = Distance(current, chosen.position);
        const int travelMs = static_cast<int>(distance / speed);
        if (static_cast<int>(requestedSafeFor) - kMoveAwayBufferMs <= travelMs) {
            const float fraction =
                distance > 0.0f
                    ? std::min(1.0f, static_cast<float>(frameMs) * speed / distance)
                    : 1.0f;
            output.x = current.x + (chosen.position.x - current.x) * fraction;
            output.y = current.y + (chosen.position.y - current.y) * fraction;
        }
    }
    return g_originalMoveUpdate ? g_originalMoveUpdate(player, output.x, -output.y) : 0;
}

// Player.Update detour. Mirrors realmsense hook_player_update: each player
// update, zero the float at props+0x788 (props = player+0x18). This neutralizes
// a per-frame movement/push value so the player stays exactly where dodge puts
// them. Done unconditionally, before the original, exactly like realmsense.
using PlayerUpdateFn = char(__fastcall*)(void*, int32_t, int32_t, void*);
PlayerUpdateFn g_originalPlayerUpdate = nullptr;

char __fastcall HookPlayerUpdate(void* self, int32_t now, int32_t delta, void* method) {
    static bool once = false;
    if (!once) { once = true; DBLOG("HookPlayerUpdate: first call self=%p", self); }

    if (self) {
        uintptr_t props = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(self) + 0x18);
        if (props > 0xFFFF)
            *reinterpret_cast<float*>(props + 0x788) = 0.0f;
    }
    return g_originalPlayerUpdate ? g_originalPlayerUpdate(self, now, delta, method) : 0;
}

} // namespace

void Install() {
    void* target = ga::Rva(kMoveUpdateRva);
    DBLOG("dodge::Install: move-hook target=%p (GA+0x%llX)", target,
          (unsigned long long)kMoveUpdateRva);
    if (target) {
        MH_STATUS st = MH_CreateHook(target, reinterpret_cast<void*>(&HookMoveUpdate),
                      reinterpret_cast<void**>(&g_originalMoveUpdate));
        DBLOG("dodge::Install: MH_CreateHook=%d orig=%p", (int)st, (void*)g_originalMoveUpdate);
    }

    void* puTarget = ga::Rva(kPlayerUpdateRva);
    DBLOG("dodge::Install: player-update target=%p (GA+0x%llX)", puTarget,
          (unsigned long long)kPlayerUpdateRva);
    if (puTarget) {
        MH_STATUS st = MH_CreateHook(puTarget, reinterpret_cast<void*>(&HookPlayerUpdate),
                      reinterpret_cast<void**>(&g_originalPlayerUpdate));
        DBLOG("dodge::Install: player-update MH_CreateHook=%d orig=%p",
              (int)st, (void*)g_originalPlayerUpdate);
    }
}

void Tick() {
    // The dodge decision is stateless per move-hook call; nothing to tick.
}

} // namespace dodge
