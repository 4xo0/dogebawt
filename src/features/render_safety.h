#pragma once

namespace render_safety {

// Present-thread overlay tick. The implementation is read-only and is gated by
// g_cfg.renderSafetyPath.
void Tick();

} // namespace render_safety
