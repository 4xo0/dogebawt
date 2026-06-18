// hooks.h - DX11 swapchain Present hook (global vtable slot swap) + WndProc.
#pragma once
#include <d3d11.h>

namespace hooks {
    // Discover the process-global IDXGISwapChain vtable via a throwaway
    // device+swapchain and overwrite slot[8] (Present) so every swapchain in
    // the process - including the game's - routes through us. Mirrors the
    // runtime installer that set qword_1801B2728 in the original.
    bool Install();
    void Uninstall();
    bool Installed();

    extern HRESULT (__stdcall* oPresent)(IDXGISwapChain*, UINT, UINT);
}
