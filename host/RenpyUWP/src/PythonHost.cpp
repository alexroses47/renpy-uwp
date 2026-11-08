// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "PythonHost.h"

// Python.h must come first (it sets up Windows/CRT macros for the interpreter build).
#include <Python.h>

#include <windows.h>

#include <atomic>
#include <string>

#include "Log.h"

namespace renpyuwp::python {

namespace {

constexpr SIZE_T kStackSize = 8 * 1024 * 1024;

std::atomic<State> g_state{State::NotStarted};
std::wstring g_localState;
std::wstring g_installDir;

bool CheckStatus(PyStatus status, const char* what) {
    if (!PyStatus_Exception(status)) return true;
    RLOG("python: %s FAILED: %s%s%s (exitcode %d)", what, status.func ? status.func : "",
         status.func ? ": " : "", status.err_msg ? status.err_msg : "?", status.exitcode);
    return false;
}

// Logs the pending Python exception with its traceback (sys.stderr may be None in UWP).
void LogPythonError(const char* where) {
    PyObject *type = nullptr, *value = nullptr, *tb = nullptr;
    PyErr_Fetch(&type, &value, &tb);
    PyErr_NormalizeException(&type, &value, &tb);
    RLOG("python: exception in %s", where);
    PyObject* tbmod = PyImport_ImportModule("traceback");
    PyObject* lines = nullptr;
    if (tbmod && type) {
        lines = PyObject_CallMethod(tbmod, "format_exception", "OOO", type, value ? value : Py_None,
                                    tb ? tb : Py_None);
    }
    if (lines && PyList_Check(lines)) {
        for (Py_ssize_t i = 0; i < PyList_GET_SIZE(lines); ++i) {
            const char* s = PyUnicode_AsUTF8(PyList_GET_ITEM(lines, i));
            if (s) RLOG("python:   %s", s);
        }
    } else {
        PyErr_Clear();
        PyObject* str = value ? PyObject_Str(value) : nullptr;
        const char* s = str ? PyUnicode_AsUTF8(str) : nullptr;
        RLOG("python:   %s", s ? s : "<no message>");
        Py_XDECREF(str);
    }
    Py_XDECREF(lines);
    Py_XDECREF(tbmod);
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(tb);
    PyErr_Clear();
}

bool SetSysString(const char* name, const std::wstring& value) {
    PyObject* v = PyUnicode_FromWideChar(value.c_str(), static_cast<Py_ssize_t>(value.size()));
    if (!v) return false;
    int rc = PySys_SetObject(name, v);
    Py_DECREF(v);
    return rc == 0;
}

int RunInterpreter() {
    PyPreConfig pre;
    PyPreConfig_InitIsolatedConfig(&pre);
    pre.utf8_mode = 1;
    if (!CheckStatus(Py_PreInitialize(&pre), "Py_PreInitialize")) return 1;

    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    // Paths relative to the package folder (the process working directory), as Kodi does:
    // absolute paths into the install folder are restricted inside an app container.
    bool ok = CheckStatus(PyConfig_SetString(&config, &config.home, L"python"), "set home") &&
              CheckStatus(PyConfig_SetString(&config, &config.program_name, L"RenpyUWP"), "set program_name");
    config.module_search_paths_set = 1;
    ok = ok &&
         CheckStatus(PyWideStringList_Append(&config.module_search_paths, L"python\\python312.zip"), "path zip") &&
         CheckStatus(PyWideStringList_Append(&config.module_search_paths, L"python"), "path dir");
    config.write_bytecode = 0;
    config.install_signal_handlers = 0;
    config.parse_argv = 0;
    config.site_import = 0;
    config.safe_path = 1;
    config.user_site_directory = 0;
    config.buffered_stdio = 0;
    if (!ok) {
        PyConfig_Clear(&config);
        return 1;
    }
    RLOG("python: Py_InitializeFromConfig (home=python, paths=python\\python312.zip;python)");
    PyStatus st = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (!CheckStatus(st, "Py_InitializeFromConfig")) return 1;
    RLOG("python: initialized: %s", Py_GetVersion());

    int rc = 1;
    if (!SetSysString("_renpy_uwp_localstate", g_localState) || !SetSysString("_renpy_uwp_install", g_installDir)) {
        LogPythonError("setting sys attributes");
    } else if (PyObject* smoke = PyImport_ImportModule("smoke")) {
        PyObject* result = PyObject_CallMethod(smoke, "main", nullptr);
        if (result) {
            rc = PyLong_Check(result) ? static_cast<int>(PyLong_AsLong(result)) : 0;
            RLOG("python: smoke.main() returned %d", rc);
            Py_DECREF(result);
        } else {
            LogPythonError("smoke.main()");
        }
        Py_DECREF(smoke);
    } else {
        LogPythonError("import smoke");
    }

    int fin = Py_FinalizeEx();
    RLOG("python: Py_FinalizeEx returned %d", fin);
    return rc;
}

DWORD WINAPI ThreadMain(LPVOID) {
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    RLOG("python: thread started, stack %llu KB", static_cast<unsigned long long>((high - low) / 1024));
    int rc = RunInterpreter();
    g_state = rc == 0 ? State::Succeeded : State::Failed;
    RLOG("python: thread finished (%s)", rc == 0 ? "success" : "failure");
    return static_cast<DWORD>(rc);
}

}  // namespace

void StartAsync(const std::wstring& localState, const std::wstring& installDir) {
    State expected = State::NotStarted;
    if (!g_state.compare_exchange_strong(expected, State::Running)) {
        RLOG("python: already started");
        return;
    }
    g_localState = localState;
    g_installDir = installDir;
    HANDLE h = CreateThread(nullptr, kStackSize, ThreadMain, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!h) {
        RLOG("python: CreateThread failed: %lu", GetLastError());
        g_state = State::Failed;
        return;
    }
    CloseHandle(h);
}

State GetState() { return g_state.load(); }

}  // namespace renpyuwp::python
