// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Log.h"

#include <windows.h>

#include <share.h>

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
    std::wstring prev = path + L"\\renpy-uwp.prev.log";
    path += L"\\renpy-uwp.log";
    // Keep the previous launch's log (a test that spans two launches stays inspectable).
    _wremove(prev.c_str());
    _wrename(path.c_str(), prev.c_str());
    // _wfopen_s opens files without sharing; the Device Portal must be able to read the log
    // while the app is running, so allow shared reads.
    g_file = _wfsopen(path.c_str(), L"wb", _SH_DENYWR);
    if (!g_file) {
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
