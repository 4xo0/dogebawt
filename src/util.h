// util.h - low-level memory patch helpers.
//
// The original wraps every GameAssembly patch in the same idiom (seen all over
// the decompilation, e.g. sub_18002CEF0):
//     p = GetModuleHandleA("GameAssembly.dll") + offset;
//     VirtualProtect(p, 256, PAGE_EXECUTE_READWRITE, &old);
//     <write bytes>
//     VirtualProtect(p, 256, old, &old);
#pragma once
#include <cstdint>
#include <cstddef>

namespace util {
    // Patch `len` bytes at addr (RWX-toggled). Faithful to the 256-byte protect
    // window the original always uses regardless of patch size.
    bool Patch(void* addr, const void* bytes, size_t len);

    template <typename T>
    bool Write(void* addr, T value) { return Patch(addr, &value, sizeof(T)); }

    template <typename T>
    T Read(void* addr) { return *reinterpret_cast<T*>(addr); }
}
