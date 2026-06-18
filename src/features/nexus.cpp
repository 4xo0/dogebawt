// nexus.cpp - latched health watcher and nexus request.
//
// The original watcher (sub_180087490) clamps percent to [0, 99.99], compares
// hp/maxHp, emits one request, and does not re-arm until health recovers or the
// player enters one of the safe-area names.  This port preserves the one-shot
// latch and calls the same GameAssembly nexus routine used by sub_1800856F0.
#include "features.h"
#include "config.h"
#include "il2cpp.h"
#include "log.h"

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include "imgui.h"

namespace nexus {
namespace {

constexpr uintptr_t kNexusRva = 0x1C9F700;
using NexusFn = int64_t (__fastcall*)(uintptr_t);

bool g_armed = true;
ULONGLONG g_messageUntil = 0;
char g_message[96]{};

void ShowMessage(const char* prefix, int hp) {
    std::snprintf(g_message, sizeof(g_message), "%s%d hp", prefix, hp);
    g_messageUntil = GetTickCount64() + 10000;
}

void ShowPercentMessage(float percent) {
    std::snprintf(g_message, sizeof(g_message),
                  "Nexusing: Too low of health: %.2f%%", percent);
    g_messageUntil = GetTickCount64() + 10000;
}

void RequestNexus(uintptr_t worldState, int predictedHp, float currentPercent,
                  bool serverSide) {
    if (!worldState)
        return;
    auto fn = reinterpret_cast<NexusFn>(ga::Rva(kNexusRva));
    if (!fn)
        return;

    // sub_1800856F0 uses root->[+0x28], not the player at [+0x28]->[+0x48].
    fn(worldState);
    g_armed = false;
    if (g_cfg.autoNexusDisplay) {
        if (serverSide)
            ShowMessage("Nexus'd: About to be at ", predictedHp);
        else
            ShowPercentMessage(currentPercent);
    }
}

void DrawMessage() {
    if (!g_cfg.autoNexusDisplay || !g_message[0] ||
        GetTickCount64() >= g_messageUntil) {
        if (GetTickCount64() >= g_messageUntil)
            g_message[0] = '\0';
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::Begin("##dogebawt_nexus", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted(g_message);
    ImGui::End();
}

} // namespace

void Install() {}

// Render thread (Present hook): draw only. The GA nexus call MUST NOT happen
// here - calling an il2cpp managed method off the game thread crashes. See Poll.
void Tick() {
    DrawMessage();
}

// Game thread (GA+WORLD_UPDATE_FN detour, via features::GameTick). This mirrors
// sub_1800856F0 firing qword_1801B27A0(worldState) on the game thread.
void Poll() {
    if (!g_cfg.autoNexus) {
        g_armed = true;
        return;
    }

    uintptr_t root = game::Root();
    uintptr_t player = game::Player();
    if (!root || !player) {
        g_armed = true;
        return;
    }

    int maxHp = *reinterpret_cast<int*>(player + 0x208);
    int hp = *reinterpret_cast<int*>(player + 0x20C);
    if (maxHp <= 0)
        return;

    // Two trigger modes, matching sub_180086D40 / sub_180087490:
    //   percent mode (dword_1801B2CFC): (percent/100) >= hp/maxHp, percent
    //                                   clamped to [0, 99.99].
    //   value mode  (else):             hp < min(hpValue, maxHp - 1).
    bool low;
    float percent = std::clamp(g_cfg.autoNexusHpPercent, 0.0f, 99.99f);
    int threshold = std::min(static_cast<int>(g_cfg.autoNexusHpValue), maxHp - 1);
    if (g_cfg.autoNexusUsePercent) {
        low = (100.0f * static_cast<float>(hp) / static_cast<float>(maxHp)) <= percent;
    } else {
        low = hp < threshold;
    }

    static ULONGLONG lastLog = 0;
    ULONGLONG now = GetTickCount64();
    if (now - lastLog > 1000) {
        lastLog = now;
        DBLOG("nexus::Poll usePercent=%d hp=%d maxHp=%d pct=%.2f curPct=%.2f thr=%d low=%d armed=%d",
              (int)g_cfg.autoNexusUsePercent, hp, maxHp, percent,
              100.0f * (float)hp / (float)maxHp, threshold, (int)low, (int)g_armed);
    }

    if (!low) {
        g_armed = true;
        return;
    }

    uintptr_t worldState = *reinterpret_cast<uintptr_t*>(root + 0x28);
    if (g_armed)
        RequestNexus(worldState, hp,
                     100.0f * static_cast<float>(hp) / static_cast<float>(maxHp),
                     false);
}

} // namespace nexus
