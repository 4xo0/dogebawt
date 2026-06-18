#include "features.h"
#include "il2cpp.h"
#include "log.h"
#include "MinHook.h"

namespace features {
    void InstallAll() {
        DBLOG("InstallAll: MH_Initialize");
        MH_Initialize();   // feature detours (move fn, speed getter, ...) use MinHook
        DBLOG("InstallAll: InstallRootCapture");
        game::InstallRootCapture();  // captures qword_1801B2E10 (needed by aim/etc.)
        DBLOG("InstallAll: InstallGameTick");
        game::InstallGameTick();     // per-frame game-thread hook (nexus / noclip auto)
        DBLOG("InstallAll: aim::Install");
        aim::Install();
        DBLOG("InstallAll: dodge::Install");
        dodge::Install();
        DBLOG("InstallAll: nexus::Install");
        nexus::Install();
        DBLOG("InstallAll: speedhack::Install");
        speedhack::Install();
        DBLOG("InstallAll: loot::Install");
        loot::Install();
        DBLOG("InstallAll: glow::Install");
        glow::Install();
        DBLOG("InstallAll: fame::Install");
        fame::Install();
        DBLOG("InstallAll: noclip::Install");
        noclip::Install();
        DBLOG("InstallAll: mods::Install");
        mods::Install();
        DBLOG("InstallAll: socketfu::Install");
        socketfu::Install();
        DBLOG("InstallAll: lagport::Install");
        lagport::Install();
        DBLOG("InstallAll: MH_EnableHook(ALL)");
        MH_STATUS st = MH_EnableHook(MH_ALL_HOOKS);
        DBLOG("InstallAll: MH_EnableHook returned %d", (int)st);
    }
    void Tick() {
        // First-frame breadcrumbs: the first Tick that crashes leaves the last
        // log line naming the feature. Subsequent frames stay quiet.
        static bool firstTick = true;
        if (firstTick) DBLOG("Tick#1: aim");
        aim::Tick();
        if (firstTick) DBLOG("Tick#1: dodge");
        dodge::Tick();
        if (firstTick) DBLOG("Tick#1: nexus");
        nexus::Tick();
        if (firstTick) DBLOG("Tick#1: speedhack");
        speedhack::Tick();
        if (firstTick) DBLOG("Tick#1: loot");
        loot::Tick();
        if (firstTick) DBLOG("Tick#1: glow");
        glow::Tick();
        if (firstTick) DBLOG("Tick#1: fame");
        fame::Tick();
        if (firstTick) DBLOG("Tick#1: noclip");
        noclip::Tick();
        if (firstTick) DBLOG("Tick#1: mods");
        mods::Tick();
        if (firstTick) DBLOG("Tick#1: hud");
        hud::Tick();
        if (firstTick) DBLOG("Tick#1: socketfu");
        socketfu::Tick();
        if (firstTick) DBLOG("Tick#1: lagport");
        lagport::Tick();
        if (firstTick) { DBLOG("Tick#1: complete"); firstTick = false; }
    }

    // Runs on the game thread (from the GA+WORLD_UPDATE_FN detour). il2cpp
    // managed calls are only safe here, not on the render/Present thread.
    void GameTick() {
        nexus::Poll();
        noclip::Poll();
        teleport::Poll();
    }
}
