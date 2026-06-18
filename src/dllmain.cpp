// dllmain.cpp - injection entry. Spawns a worker that waits for GameAssembly,
// installs the DX11 + feature hooks, then idles until unload.
#include <windows.h>
#include <atomic>

#include "hooks.h"
#include "il2cpp.h"
#include "config.h"
#include "log.h"

static HMODULE g_self = nullptr;
static std::atomic<bool> g_stopping{false};

static DWORD WINAPI Worker(LPVOID) {
    DBLOG("Worker: start, waiting for GameAssembly.dll");
    // Wait for the il2cpp game image to be present before touching it.
    while (!g_stopping.load(std::memory_order_acquire) && !ga::Init())
        Sleep(50);
    if (g_stopping.load(std::memory_order_acquire))
        return 0;
    DBLOG("Worker: GameAssembly base = %p", (void*)ga::Base());

    DBLOG("Worker: Config_Load()");
    Config_Load();
    DBLOG("Worker: Config_Load() done");

    // The original installs GameAssembly hooks lazily from Present. This
    // worker only establishes the process-global Present vtable hook.
    DBLOG("Worker: hooks::Install()");
    while (!g_stopping.load(std::memory_order_acquire) && !hooks::Install())
        Sleep(250);
    DBLOG("Worker: hooks::Install() returned, idle");

    return 0;
}

extern "C" __declspec(dllexport) void DogeBawt_Shutdown() {
    g_stopping.store(true, std::memory_order_release);
    hooks::Uninstall();
    ga::Reset();
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hinst;
        DisableThreadLibraryCalls(hinst);
        dlog::Init();
        DBLOG("DllMain: DLL_PROCESS_ATTACH, spawning worker");
        HANDLE worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (worker)
            CloseHandle(worker);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_stopping.store(true, std::memory_order_release);
        // Process termination tears down D3D/Win32 itself. For explicit
        // FreeLibrary, callers must invoke DogeBawt_Shutdown first so cleanup
        // runs outside the loader lock.
        if (reserved)
            game::ResetRuntimeState();
    }
    return TRUE;
}
