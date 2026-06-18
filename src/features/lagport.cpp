// lagport.cpp - "Lag Port": freeze the client while a hotkey is held.
//
// In the original the per-frame game update (GA+0xDA6830, sub_1800856F0) is
// skipped while its freeze gate is set (`!word_1801B2DE0`). We reproduce the
// behavior: while Lag Port is enabled and its hotkey is held, FreezeActive()
// returns true and hkGameTick suppresses the original update, so the client
// stops processing/updating ("lags"). Releasing the key resumes and the client
// snaps to the server state. As the menu warns: do not use near projectiles -
// while frozen you cannot react and may die.
#include "features.h"
#include "config.h"

#include <windows.h>

namespace lagport {
namespace {

bool KeyDown(int vk) {
    return vk > 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
}

} // namespace

bool FreezeActive() {
    return g_cfg.lagPort && KeyDown(g_cfg.lagPortHotkey.vk);
}

void Install() {}
void Tick() {}

} // namespace lagport
