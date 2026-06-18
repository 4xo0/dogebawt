// socketfu.cpp - "SocketFu": phase through walls (noclip) while switched to the
// second speed slot, with an optional movement-speed cap so the client doesn't
// outrun 1.5x and desync.
//
//   * move-speed cap  sub_180085690 (GA+0xA44B00, qword_1801B2810): when socket
//     is active and "restrict movement" is on, if the active speedhack
//     multiplier > 1.5 the move speed is scaled by 1.5/mult.
//   * noclip          reuses noclip::SetManual (the shared gate/patch).
//   * speed slot      engaging switches to Speed 2 (the original sets
//     unk_1801B31CD = 0 when "use second speed" is set).
//
// The original triggers SocketFu from a hotkey; our menu models it as the
// g_cfg.socketFu toggle, so "active" == that checkbox.
#include "features.h"
#include "config.h"
#include "il2cpp.h"
#include "log.h"

#include <windows.h>
#include <cstdint>
#include "imgui.h"
#include "MinHook.h"

namespace socketfu {
namespace {

constexpr uintptr_t kMoveSpeedRva = 0xA44B00; // qword_1801B2810 (sub_180085690)

using SpeedFn = float(__fastcall*)(uintptr_t);
SpeedFn g_orig = nullptr;

bool      g_active = false;   // socket currently engaged (read by the GA hook)
bool      g_prev = false;     // previous toggle state, for edge detection
ULONGLONG g_sinceMs = 0;      // engage time, for the timer overlay

// sub_180085690: cap the move speed to 1.5x while socketed + restrict-movement,
// when the active speedhack multiplier exceeds 1.5.
float __fastcall hkMoveSpeed(uintptr_t self) {
    float r = g_orig ? g_orig(self) : 0.0f;
    if (g_active && g_cfg.socketFuRestrictMovement) {
        const float mult = speedhack::CurrentSpeed();
        if (mult > 1.5f)
            r = (1.5f / mult) * r;
    }
    return r;
}

} // namespace

void Install() {
    void* target = ga::Rva(kMoveSpeedRva);
    DBLOG("socketfu::Install: move-speed target=%p (GA+0x%llX)", target,
          (unsigned long long)kMoveSpeedRva);
    if (target) {
        const MH_STATUS st = MH_CreateHook(target, reinterpret_cast<void*>(&hkMoveSpeed),
                                           reinterpret_cast<void**>(&g_orig));
        DBLOG("socketfu::Install: MH_CreateHook=%d", (int)st);
    }
}

void Tick() {
    const bool active = g_cfg.socketFu;

    // On engage: start the timer and switch to the second speed slot (matches
    // the original setting unk_1801B31CD = 0 when "use second speed" is on).
    if (active && !g_prev) {
        g_sinceMs = GetTickCount64();
        if (g_cfg.socketFuUseSecondSpeed)
            g_cfg.useSpeed1 = false;
    }
    g_prev = active;
    g_active = active;

    // Phase through walls while socketed.
    noclip::SetManual(active && g_cfg.socketFuNoClip);

    if (active && g_cfg.showSocketFuTimer) {
        const double secs = static_cast<double>(GetTickCount64() - g_sinceMs) / 1000.0;
        ImGui::SetNextWindowBgAlpha(0.40f);
        ImGui::Begin("##dogebawt_socketfu", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::Text("SocketFU: %.1f", secs);
        ImGui::End();
    }
}

} // namespace socketfu
