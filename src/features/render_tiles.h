#pragma once

namespace render_tiles {

// Render-thread entry point. Call after ImGui::NewFrame().
void Tick();

} // namespace render_tiles
