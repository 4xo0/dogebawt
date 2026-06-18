// log.h - crash-tracing logger.
//
// Writes one flushed line per call to both OutputDebugStringA (visible in
// DebugView / a debugger) and a log file, so the LAST line in the file names
// the step that was executing when the process died. Flush-after-every-line is
// the whole point: an access violation must not lose the breadcrumb.
#pragma once

namespace dlog {
    // Opens the log file. Safe to call more than once. The resolved path is
    // emitted via OutputDebugStringA so it can be found at runtime.
    void Init();
    void Shutdown();

    // printf-style; appends a newline. Thread-safe.
    void Write(const char* fmt, ...);
}

#define DBLOG(...) ::dlog::Write(__VA_ARGS__)
