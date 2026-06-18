// hud.cpp - on-screen text overlays the original draws via the game's text
// renderer in sub_1800D5500 ("Speed: ", "FPM: "). In our port these are drawn
// with ImGui instead (render thread). Fame-per-minute is computed from the
// player's run-fame field at player+0x550 (1360), exactly the field
// sub_1800D5500 reads (*(player+1360)).
#include "features.h"
#include "config.h"
#include "il2cpp.h"

#include "imgui.h"
#include <windows.h>
#include <cstdio>

namespace hud {
namespace {

constexpr int kFameOffset = 0x550; // player+1360, read by sub_1800D5500

bool      g_haveBase = false;
int       g_baseFame = 0;
ULONGLONG g_baseTime = 0;
float     g_fpm = 0.0f;

int PlayerFame() {
    const uintptr_t player = game::Player();
    if (!player)
        return -1;
    return *reinterpret_cast<int*>(player + kFameOffset);
}

void UpdateFpm() {
    const int fame = PlayerFame();
    if (fame < 0) {                 // left world / no player -> rebase next time
        g_haveBase = false;
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (!g_haveBase || fame < g_baseFame) { // fresh run or fame reset
        g_baseFame = fame;
        g_baseTime = now;
        g_haveBase = true;
        g_fpm = 0.0f;
        return;
    }
    const double minutes = static_cast<double>(now - g_baseTime) / 60000.0;
    if (minutes > 1.0 / 600.0 && fame != g_baseFame)
        g_fpm = static_cast<float>((fame - g_baseFame) / minutes);
}

} // namespace

void Tick() {
    const bool showSpeed = g_cfg.showCurrentSpeed;
    const bool showFpm = g_cfg.showFpm;
    if (showFpm)
        UpdateFpm();
    else
        g_haveBase = false;

    if (!showSpeed && !showFpm)
        return;

    ImGui::SetNextWindowBgAlpha(0.40f);
    ImGui::Begin("##dogebawt_hud", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing);
    if (showSpeed)
        ImGui::Text("Speed: %.2f", speedhack::CurrentSpeed());
    if (showFpm)
        ImGui::Text("FPM: %.0f", g_fpm);
    ImGui::End();
}

} // namespace hud
