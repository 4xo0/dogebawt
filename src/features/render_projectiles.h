#pragma once

namespace render_projectiles {

// No hooks are required. Install is provided for feature-orchestrator symmetry.
void Install();

// Called from the ImGui/Present thread after ImGui::NewFrame().
void Tick();

} // namespace render_projectiles
