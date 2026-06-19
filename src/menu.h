#pragma once

struct ImFont;

namespace menu {
    void SetFonts(ImFont* normal, ImFont* bold);
    void Render();
    void Toggle();
    void SetOpen(bool open);
    bool IsOpen();
    void ApplyTheme(int theme);
}
