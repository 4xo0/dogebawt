#pragma once

namespace render_hitbox {

// Render-thread overlay tick. Draws the player's dodge collision box when
// g_cfg.renderHitbox is enabled.
void Tick();

} // namespace render_hitbox
