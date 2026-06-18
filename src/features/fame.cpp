// fame.cpp - fame/account-fame overrides and current fame-per-minute display.
#include "features.h"
#include "config.h"
#include "il2cpp.h"

#include <windows.h>
#include <cstdint>
#include <cstring>
#include "MinHook.h"
#include "imgui.h"

namespace fame {
namespace {

constexpr uintptr_t kStatsUpdateRva = 0xA37390;
constexpr uintptr_t kFameWritePatchRva = 0xA484FF;

using StatsUpdateFn = int64_t (__fastcall*)(uintptr_t, uintptr_t);
StatsUpdateFn oStatsUpdate = nullptr;
int g_selectedObjectId = -1;

uint8_t g_patchBackup[6]{};
bool g_havePatchBackup = false;
bool g_patchEnabled = false;

bool ReadList(uintptr_t list, uintptr_t& data, int& count) {
    if (!list)
        return false;
    data = *reinterpret_cast<uintptr_t*>(list + 0x10);
    count = *reinterpret_cast<int*>(list + 0x18);
    return data && count > 0 && count <= 500;
}

void RewriteAccountFame(uintptr_t packet) {
    if (!packet || !g_cfg.accountFame)
        return;

    uintptr_t outerData = 0;
    int outerCount = 0;
    if (!ReadList(*reinterpret_cast<uintptr_t*>(packet + 0x28), outerData, outerCount))
        return;

    for (int i = 0; i < outerCount; ++i) {
        uintptr_t objectUpdate = *reinterpret_cast<uintptr_t*>(outerData + 0x20 + 8ull * i);
        if (!objectUpdate)
            continue;
        uintptr_t update = *reinterpret_cast<uintptr_t*>(objectUpdate + 0x18);
        if (!update)
            continue;
        uintptr_t stats = *reinterpret_cast<uintptr_t*>(update + 0x20);
        if (!stats || *reinterpret_cast<int*>(update + 0x10) != g_selectedObjectId)
            continue;

        uintptr_t statData = 0;
        int statCount = 0;
        if (!ReadList(stats, statData, statCount))
            continue;
        for (int j = 0; j < statCount; ++j) {
            uintptr_t stat = *reinterpret_cast<uintptr_t*>(statData + 0x20 + 8ull * j);
            if (stat && *reinterpret_cast<int*>(stat + 0x10) == 39) {
                *reinterpret_cast<int*>(stat + 0x14) =
                    static_cast<int>(g_cfg.accountFameValue);
            }
        }
    }
}

int64_t __fastcall hkStatsUpdate(uintptr_t self, uintptr_t packet) {
    if (packet) {
        uintptr_t descriptor = *reinterpret_cast<uintptr_t*>(packet);
        uintptr_t kindPtr = descriptor ? *reinterpret_cast<uintptr_t*>(descriptor + 0x318) : 0;
        uint8_t kind = kindPtr ? *reinterpret_cast<uint8_t*>(kindPtr + 1) : 0;
        if (kind == 92)
            g_selectedObjectId = -1;
        else if (kind == 101)
            g_selectedObjectId = *reinterpret_cast<int*>(packet + 0x10);
        else if (kind == 42)
            RewriteAccountFame(packet);
    }
    return oStatsUpdate ? oStatsUpdate(self, packet) : 0;
}

void SetFamePatch(bool enable) {
    if (enable == g_patchEnabled)
        return;
    uint8_t* site = static_cast<uint8_t*>(ga::Rva(kFameWritePatchRva));
    if (!site)
        return;

    DWORD oldProtect = 0;
    if (!VirtualProtect(site, sizeof(g_patchBackup), PAGE_EXECUTE_READWRITE, &oldProtect))
        return;
    if (enable) {
        if (!g_havePatchBackup) {
            memcpy(g_patchBackup, site, sizeof(g_patchBackup));
            g_havePatchBackup = true;
        }
        memset(site, 0x90, sizeof(g_patchBackup));
    } else if (g_havePatchBackup) {
        memcpy(site, g_patchBackup, sizeof(g_patchBackup));
    }
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(g_patchBackup));
    DWORD ignored = 0;
    VirtualProtect(site, sizeof(g_patchBackup), oldProtect, &ignored);
    g_patchEnabled = enable;
}

void DrawFpm(uintptr_t player) {
    static bool tracking = false;
    static int startFame = 0;
    static LARGE_INTEGER start{};
    static double fpm = 0.0;

    if (!g_cfg.showFpm || !player) {
        tracking = false;
        return;
    }

    int currentFame = *reinterpret_cast<int*>(player + 0x550);
    LARGE_INTEGER now{}, freq{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    if (!tracking) {
        tracking = true;
        startFame = currentFame;
        start = now;
        fpm = 0.0;
    }

    double seconds = freq.QuadPart
        ? static_cast<double>(now.QuadPart - start.QuadPart) /
              static_cast<double>(freq.QuadPart)
        : 0.0;
    int gained = currentFame - startFame;
    if (seconds > 0.0 && gained != 0 && !g_cfg.fameValue)
        fpm = static_cast<double>(gained) / (seconds / 60.0);
    else
        fpm = 0.0;

    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##dogebawt_fpm", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("FPM: %.2f", fpm);
    ImGui::End();
}

} // namespace

void Install() {
    void* target = ga::Rva(kStatsUpdateRva);
    if (target)
        MH_CreateHook(target, reinterpret_cast<void*>(&hkStatsUpdate),
                      reinterpret_cast<void**>(&oStatsUpdate));
}

void Tick() {
    uintptr_t player = game::Player();
    SetFamePatch(g_cfg.fameValue);

    if (player && g_cfg.fameValue) {
        int value = static_cast<int>(g_cfg.fameValueAmount);
        *reinterpret_cast<int*>(player + 0x550) = value;
        *reinterpret_cast<int*>(player + 0x548) = value;
    }

    DrawFpm(player);
}

} // namespace fame
