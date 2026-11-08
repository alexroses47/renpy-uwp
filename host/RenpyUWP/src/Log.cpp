#include "Log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

namespace renpyuwp::log {

namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
}  // namespace

void Init() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        return;
    }
    std::wstring path;
    try {
        path = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path().c_str();
    } catch (...) {
        OutputDebugStringW(L"renpy-uwp: cannot resolve LocalState path\n");
        return;
    }
    path += L"\\renpy-uwp.log";
    if (_wfopen_s(&g_file, path.c_str(), L"wb") != 0) {
        g_file = nullptr;
        OutputDebugStringW(L"renpy-uwp: cannot open log file\n");
    }
}

void Write(const char* fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[2200];
    snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u.%03u %s\r\n", t.wYear, t.wMonth, t.wDay,
             t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, message);

    OutputDebugStringA(line);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fputs(line, g_file);
        fflush(g_file);
    }
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

}  // namespace renpyuwp::log
