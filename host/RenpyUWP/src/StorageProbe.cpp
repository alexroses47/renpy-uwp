// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "StorageProbe.h"

#include <windows.h>

#include <fileapifromapp.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include "Log.h"

using namespace winrt;
using namespace winrt::Windows::Storage;

namespace renpyuwp::storage {

namespace {

constexpr wchar_t kTestFile[] = L"renpy-uwp-test.txt";
constexpr int kMaxEntries = 40;
std::atomic<bool> g_running{false};

std::string Printable(const char* data, size_t n) {
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c >= 0x20) out += static_cast<char>(c);  // keep UTF-8 bytes as-is
        else {
            char hex[8];
            snprintf(hex, sizeof(hex), "\\x%02x", c);
            out += hex;
        }
    }
    return out;
}

const char* DriveTypeName(UINT t) {
    switch (t) {
        case DRIVE_UNKNOWN: return "unknown";
        case DRIVE_NO_ROOT_DIR: return "no-root";
        case DRIVE_REMOVABLE: return "removable";
        case DRIVE_FIXED: return "fixed";
        case DRIVE_REMOTE: return "remote";
        case DRIVE_CDROM: return "cdrom";
        case DRIVE_RAMDISK: return "ramdisk";
        default: return "?";
    }
}

void LogVolume(const std::wstring& root) {
    UINT type = GetDriveTypeW(root.c_str());
    wchar_t label[MAX_PATH + 1] = L"", fs[MAX_PATH + 1] = L"";
    DWORD serial = 0, maxComp = 0, flags = 0;
    BOOL vol = GetVolumeInformationW(root.c_str(), label, MAX_PATH, &serial, &maxComp, &flags, fs, MAX_PATH);
    DWORD volErr = vol ? 0 : GetLastError();
    ULARGE_INTEGER freeBytes{}, total{}, totalFree{};
    BOOL space = GetDiskFreeSpaceExW(root.c_str(), &freeBytes, &total, &totalFree);
    DWORD spaceErr = space ? 0 : GetLastError();
    char volInfo[160], spaceInfo[96];
    if (vol) snprintf(volInfo, sizeof(volInfo), "label='%ls' fs=%ls serial=%08lx", label, fs, serial);
    else snprintf(volInfo, sizeof(volInfo), "GetVolumeInformation error %lu", volErr);
    if (space) snprintf(spaceInfo, sizeof(spaceInfo), "%llu MB free / %llu MB", freeBytes.QuadPart >> 20, total.QuadPart >> 20);
    else snprintf(spaceInfo, sizeof(spaceInfo), "GetDiskFreeSpaceEx error %lu", spaceErr);
    RLOG("storage:   %ls type=%s %s, %s", root.c_str(), DriveTypeName(type), volInfo, spaceInfo);
}

using FindFirstFn = HANDLE(WINAPI*)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);

// Lists a directory with the given FindFirstFileEx variant. Returns entry count or -1.
int ListDir(const std::wstring& dir, FindFirstFn findFirst, const char* api, bool logEntries) {
    std::wstring pattern = dir + L"*";
    WIN32_FIND_DATAW fd{};
    HANDLE h = findFirst(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) {
        RLOG("storage:   list %ls via %s: FAILED, error %lu", dir.c_str(), api, GetLastError());
        return -1;
    }
    int n = 0;
    std::vector<std::wstring> lines;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        ++n;
        if (logEntries && n <= kMaxEntries) {
            wchar_t line[MAX_PATH + 64];
            ULONGLONG size = (static_cast<ULONGLONG>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) swprintf(line, MAX_PATH + 64, L"[DIR]  %ls", fd.cFileName);
            else swprintf(line, MAX_PATH + 64, L"%12llu  %ls", size, fd.cFileName);
            lines.emplace_back(line);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    RLOG("storage:   list %ls via %s: OK, %d entries", dir.c_str(), api, n);
    for (auto const& l : lines) RLOG("storage:     %ls", l.c_str());
    if (logEntries && n > kMaxEntries) RLOG("storage:     ... (%d more)", n - kMaxEntries);
    return n;
}

void LogContent(const char* api, const char* data, size_t n) {
    RLOG("storage:   read via %s: OK, %zu bytes: \"%s\"", api, n, Printable(data, n).c_str());
}

void ReadWithHandle(HANDLE h, const char* api, DWORD openErr) {
    if (h == INVALID_HANDLE_VALUE) {
        RLOG("storage:   read via %s: FAILED to open, error %lu", api, openErr);
        return;
    }
    char buf[1024];
    DWORD got = 0;
    if (ReadFile(h, buf, sizeof(buf), &got, nullptr)) LogContent(api, buf, got);
    else RLOG("storage:   read via %s: ReadFile error %lu", api, GetLastError());
    CloseHandle(h);
}

// Returns true if the test file exists on this drive (according to any API).
bool TryTestFile(const std::wstring& root) {
    std::wstring path = root + kTestFile;
    WIN32_FILE_ATTRIBUTE_DATA ad{};
    BOOL existsFromApp = GetFileAttributesExFromAppW(path.c_str(), GetFileExInfoStandard, &ad);
    DWORD errFromApp = existsFromApp ? 0 : GetLastError();
    BOOL existsWin32 = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &ad);
    DWORD errWin32 = existsWin32 ? 0 : GetLastError();
    if (!existsFromApp && !existsWin32) {
        if (errFromApp != ERROR_FILE_NOT_FOUND || errWin32 != ERROR_FILE_NOT_FOUND) {
            RLOG("storage:   %ls: not visible (FromApp error %lu, Win32 error %lu)", path.c_str(), errFromApp, errWin32);
        }
        return false;
    }
    RLOG("storage:   FOUND %ls (attributes via FromApp=%s, Win32=%s)", path.c_str(), existsFromApp ? "ok" : "fail",
         existsWin32 ? "ok" : "fail");

    HANDLE h = CreateFile2FromAppW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
    ReadWithHandle(h, "CreateFile2FromAppW", GetLastError());

    h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ReadWithHandle(h, "CreateFileW (plain Win32, what CPython uses)", GetLastError());

    FILE* f = nullptr;
    errno_t e = _wfopen_s(&f, path.c_str(), L"rb");
    if (e == 0 && f) {
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        LogContent("_wfopen_s (CRT)", buf, n);
    } else {
        RLOG("storage:   read via _wfopen_s (CRT): FAILED, errno %d", static_cast<int>(e));
    }
    return true;
}

void ProbeKnownFolders() {
    try {
        auto folders = KnownFolders::RemovableDevices().GetFoldersAsync().get();
        RLOG("storage: KnownFolders.RemovableDevices: %u folders", folders.Size());
        for (auto const& folder : folders) {
            RLOG("storage:   folder name='%ls' path='%ls'", folder.Name().c_str(), folder.Path().c_str());
            try {
                auto files = folder.GetFilesAsync().get();
                RLOG("storage:     %u files visible to WinRT (only declared file types)", files.Size());
                uint32_t i = 0;
                for (auto const& file : files) {
                    if (++i > kMaxEntries) break;
                    RLOG("storage:     %ls", file.Name().c_str());
                }
                auto item = folder.TryGetItemAsync(kTestFile).get();
                if (item) {
                    auto text = FileIO::ReadTextAsync(item.as<StorageFile>()).get();
                    RLOG("storage:     read %ls via WinRT FileIO: \"%s\"", kTestFile,
                         Printable(winrt::to_string(text).c_str(), winrt::to_string(text).size()).c_str());
                }
            } catch (hresult_error const& e) {
                RLOG("storage:     WinRT access failed 0x%08lx: %ls", static_cast<unsigned long>(e.code()), e.message().c_str());
            }
        }
    } catch (hresult_error const& e) {
        RLOG("storage: KnownFolders.RemovableDevices failed 0x%08lx: %ls", static_cast<unsigned long>(e.code()),
             e.message().c_str());
    }
}

void Probe() {
    RLOG("storage: probe start");
    DWORD mask = GetLogicalDrives();
    std::vector<wchar_t> letters;
    if (mask == 0) {
        RLOG("storage: GetLogicalDrives returned 0 (error %lu); probing C..Z by attributes", GetLastError());
        for (wchar_t c = L'C'; c <= L'Z'; ++c) {
            wchar_t root[] = {c, L':', L'\\', 0};
            WIN32_FILE_ATTRIBUTE_DATA ad{};
            if (GetFileAttributesExFromAppW(root, GetFileExInfoStandard, &ad) || GetLastError() != ERROR_PATH_NOT_FOUND) {
                letters.push_back(c);
            }
        }
    } else {
        for (int i = 0; i < 26; ++i) {
            if (mask & (1u << i)) letters.push_back(static_cast<wchar_t>(L'A' + i));
        }
    }
    std::wstring list;
    for (wchar_t c : letters) {
        list += c;
        list += L": ";
    }
    RLOG("storage: drives: %ls(mask 0x%08lx)", list.c_str(), mask);

    int found = 0;
    for (wchar_t c : letters) {
        std::wstring root = {c, L':', L'\\'};
        LogVolume(root);
        // Full listing via the FromApp API; count-only via plain Win32 to compare access.
        ListDir(root, FindFirstFileExFromAppW, "FindFirstFileExFromAppW", true);
        ListDir(root, FindFirstFileExW, "FindFirstFileExW (plain Win32)", false);
        if (TryTestFile(root)) ++found;
    }
    if (found == 0) RLOG("storage: %ls was not found on any drive", kTestFile);

    ProbeKnownFolders();
    RLOG("storage: probe done");
}

}  // namespace

void RunAsync() {
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) {
        RLOG("storage: probe already running");
        return;
    }
    std::thread([] {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        try {
            Probe();
        } catch (hresult_error const& e) {
            RLOG("storage: probe aborted 0x%08lx: %ls", static_cast<unsigned long>(e.code()), e.message().c_str());
        } catch (...) {
            RLOG("storage: probe aborted (unknown exception)");
        }
        winrt::uninit_apartment();
        g_running = false;
    }).detach();
}

}  // namespace renpyuwp::storage
