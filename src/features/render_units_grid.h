#pragma once

// Present/render-thread entry point. This only snapshots native fields and
// emits ImGui primitives; it does not call into managed GameAssembly code.
namespace render_units_grid {
    void Tick();
}
