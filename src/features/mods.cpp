// mods.cpp - small GA-hook mods that are single, self-contained detours:
//   * Anti-Idle   (sub_180085560, GA+0x1A67620): block the AFK/idle handler.
//   * Camera Zoom (sub_180085650, GA+0x163DD00): scale the camera zoom value.
#include "features.h"
#include "config.h"
#include "il2cpp.h"
#include "log.h"

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include "MinHook.h"

namespace mods {
namespace {

// Verified against DB_InstallHooks (decimal offsets -> hex via tool).
constexpr uintptr_t kAntiIdleRva   = 0x1A67620; // qword_1801B27E8 (base+27686432)
constexpr uintptr_t kCameraZoomRva = 0x163DD00; // qword_1801B27A8 (base+23321856)
constexpr float kZoomMin = 0.01f; // dword_180140E10
constexpr float kZoomMax = 4.0f;  // dword_180140F70

// Forward up to four integer slots (rcx/rdx/r8/r9). Any xmm args pass through
// untouched since we never touch the xmm registers here.
using PassFn = int64_t(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
PassFn g_origAntiIdle = nullptr;

// GA+0x163DD00 is a zoom setter: arg0 in rcx (this), the zoom float in xmm1.
// a2/a3 (r8/r9) are forwarded in case the setter takes extra integer args.
using ZoomFn = int64_t(__fastcall*)(uintptr_t, float, uintptr_t, uintptr_t);
ZoomFn g_origZoom = nullptr;

// sub_180085560: `if (!flag) return orig(); return result;` - when Anti-Idle is
// on the original idle handler is skipped entirely.
int64_t __fastcall hkAntiIdle(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    if (g_cfg.antiIdle)
        return 0;
    return g_origAntiIdle ? g_origAntiIdle(a0, a1, a2, a3) : 0;
}

// sub_180085650: xmm1 *= min(4.0, max(0.01, cameraZoomScale)); tail-call orig.
// Scale 1.0 (default) leaves the zoom unchanged.
int64_t __fastcall hkCameraZoom(uintptr_t self, float zoom, uintptr_t a2, uintptr_t a3) {
    const float scale = std::min(kZoomMax, std::max(kZoomMin, g_cfg.cameraZoomScale));
    return g_origZoom ? g_origZoom(self, zoom * scale, a2, a3) : 0;
}

} // namespace

void Install() {
    void* ai = ga::Rva(kAntiIdleRva);
    DBLOG("mods::Install: anti-idle target=%p (GA+0x%llX)", ai,
          (unsigned long long)kAntiIdleRva);
    if (ai) {
        const MH_STATUS st = MH_CreateHook(ai, reinterpret_cast<void*>(&hkAntiIdle),
                                           reinterpret_cast<void**>(&g_origAntiIdle));
        DBLOG("mods::Install: anti-idle MH_CreateHook=%d", (int)st);
    }

    void* cz = ga::Rva(kCameraZoomRva);
    DBLOG("mods::Install: camera-zoom target=%p (GA+0x%llX)", cz,
          (unsigned long long)kCameraZoomRva);
    if (cz) {
        const MH_STATUS st = MH_CreateHook(cz, reinterpret_cast<void*>(&hkCameraZoom),
                                           reinterpret_cast<void**>(&g_origZoom));
        DBLOG("mods::Install: camera-zoom MH_CreateHook=%d", (int)st);
    }
}

void Tick() {}

} // namespace mods
