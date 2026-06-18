// glow.cpp - native outline/glow material colors.
//
// sub_1800D5280 walks player->[+0x10]->[+0x60].  The material block stores
// outline RGBA at float indices 25..28 and glow RGBA at 29..32.  Disabling the
// feature deliberately does not restore anything; the original also simply
// stops writing and lets the game replace the material on its next refresh.
#include "features.h"
#include "config.h"
#include "il2cpp.h"

#include <cstdint>

namespace glow {
namespace {

struct Rgba {
    float r, g, b, a;
};

// Defaults recovered from dword_1801AFB98..A4 and 1801AFBF8..C04.
constexpr Rgba kOutline{1.0f, 1.0f, 0.5f, 0.8f};
constexpr Rgba kGlow{0.0f, 1.0f, 0.5f, 0.8f};

} // namespace

void Install() {}

void Tick() {
    if (!g_cfg.enableGlow)
        return;

    uintptr_t player = game::Player();
    if (!player)
        return;
    uintptr_t visual = *reinterpret_cast<uintptr_t*>(player + 0x10);
    if (!visual)
        return;
    float* material = *reinterpret_cast<float**>(visual + 0x60);
    if (!material)
        return;

    material[25] = kOutline.r;
    material[26] = kOutline.g;
    material[27] = kOutline.b;
    material[28] = kOutline.a;
    material[29] = kGlow.r;
    material[30] = kGlow.g;
    material[31] = kGlow.b;
    material[32] = kGlow.a;
}

} // namespace glow
