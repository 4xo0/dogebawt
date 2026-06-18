#include "il2cpp.h"
#include "log.h"
#include <windows.h>
#include <atomic>
#include "MinHook.h"

namespace ga {
    static std::atomic<uintptr_t> g_base{0};

    uintptr_t Base() {
        uintptr_t base = g_base.load(std::memory_order_acquire);
        if (!base) {
            base = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
            if (base)
                g_base.store(base, std::memory_order_release);
        }
        return base;
    }

    bool Init() { return Base() != 0; }
    bool Ready() { return g_base.load(std::memory_order_acquire) != 0; }
    void Reset() { g_base.store(0, std::memory_order_release); }
}

// Forward declarations into the feature layer (avoids a heavy include here).
namespace features { void GameTick(); }
namespace noclip   { bool GateActive(); }

namespace game {
    static std::atomic<uintptr_t> g_root{0};
    static std::atomic<bool> g_rootCaptureInstalled{false};
    static std::atomic<bool> g_gameTickInstalled{false};

    uintptr_t Root() { return g_root.load(std::memory_order_acquire); }
    void CaptureRoot(uintptr_t world) {
        // DB_CaptureRoot stores every value, including null, before forwarding.
        g_root.store(world, std::memory_order_release);
    }
    void ResetRuntimeState() { CaptureRoot(0); }

    // Root()->[+40]->[+72] (the local player object), matching the original's
    // navigation in sub_180085FC0 / sub_18002D1A0.
    uintptr_t Player() {
        const uintptr_t root = Root();
        if (!root) return 0;
        uintptr_t a = *reinterpret_cast<uintptr_t*>(root + 0x28);
        if (!a) return 0;
        return *reinterpret_cast<uintptr_t*>(a + 0x48);
    }

    void MoveTo(uintptr_t player, float x, float y) {
        if (!player) return;
        struct Vec2 { float x, y; };
        using Fn = intptr_t(__fastcall*)(uintptr_t, const Vec2*);
        auto fn = reinterpret_cast<Fn>(ga::Rva(ga::rva::MOVETO));
        if (!fn) return;
        const Vec2 position{x, y};
        fn(player, &position);
    }

    // Root capture: detour GA+WORLD_ROOT_FN; first arg is the World 'this'
    // (sub_180085800 does exactly `qword_1801B2E10 = a1; return orig(a1);`).
    static uintptr_t (__fastcall* oWorldFn)(uintptr_t) = nullptr;
    static uintptr_t __fastcall hkWorldFn(uintptr_t a1) {
        static bool once = false;
        const bool first = !once;
        if (first) { once = true; DBLOG("hkWorldFn: first call, root=%p, before orig=%p", (void*)a1, (void*)oWorldFn); }
        CaptureRoot(a1);
        uintptr_t r = oWorldFn ? oWorldFn(a1) : 0;
        if (first) DBLOG("hkWorldFn: first call returned from orig");
        return r;
    }

    void InstallRootCapture() {
        if (g_rootCaptureInstalled.load(std::memory_order_acquire))
            return;

        void* target = ga::Rva(ga::rva::WORLD_ROOT_FN);
        DBLOG("InstallRootCapture: target=%p (GA+0x%llX)", target,
              (unsigned long long)ga::rva::WORLD_ROOT_FN);
        if (!target)
            return;

        const MH_STATUS status = MH_CreateHook(target, (void*)&hkWorldFn, (void**)&oWorldFn);
        DBLOG("InstallRootCapture: MH_CreateHook=%d orig=%p", (int)status, (void*)oWorldFn);
        if (status == MH_OK)
            g_rootCaptureInstalled.store(true, std::memory_order_release);
    }

    bool RootCaptureInstalled() {
        return g_rootCaptureInstalled.load(std::memory_order_acquire);
    }

    // Per-frame game-thread update detour (GA+WORLD_UPDATE_FN). sub_1800856F0
    // does feature work, then `if (!gates) return orig(a1); return result;` -
    // i.e. when the noclip gate is up the original update is skipped too.
    static uintptr_t (__fastcall* oGameTick)(uintptr_t) = nullptr;
    static uintptr_t __fastcall hkGameTick(uintptr_t a1) {
        static bool once = false;
        if (!once) { once = true; DBLOG("hkGameTick: first call a1=%p", (void*)a1); }
        features::GameTick();
        if (noclip::GateActive())
            return 0;                       // noclip: suppress this frame's update
        return oGameTick ? oGameTick(a1) : 0;
    }

    void InstallGameTick() {
        if (g_gameTickInstalled.load(std::memory_order_acquire))
            return;

        void* target = ga::Rva(ga::rva::WORLD_UPDATE_FN);
        DBLOG("InstallGameTick: target=%p (GA+0x%llX)", target,
              (unsigned long long)ga::rva::WORLD_UPDATE_FN);
        if (!target)
            return;

        const MH_STATUS status = MH_CreateHook(target, (void*)&hkGameTick, (void**)&oGameTick);
        DBLOG("InstallGameTick: MH_CreateHook=%d orig=%p", (int)status, (void*)oGameTick);
        if (status == MH_OK)
            g_gameTickInstalled.store(true, std::memory_order_release);
    }

    bool GameTickInstalled() {
        return g_gameTickInstalled.load(std::memory_order_acquire);
    }
}
