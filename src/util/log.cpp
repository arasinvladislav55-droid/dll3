#include "util/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace uv {

void Log(const char* fmt, ...) {
    static std::mutex mutex;
    static FILE* file = nullptr;
    static bool tried = false;

    std::lock_guard<std::mutex> lock(mutex);
    if (!tried) {
        tried = true;
        wchar_t path[MAX_PATH] = {};
        if (GetTempPathW(MAX_PATH, path)) {
            wcscat_s(path, L"UniversalVisuals.log");
            _wfopen_s(&file, path, L"a");
        }
    }
    if (!file) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(file, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);
    std::fputc('\n', file);
    std::fflush(file);
}

}  // namespace uv
