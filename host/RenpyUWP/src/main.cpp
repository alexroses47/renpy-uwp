// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Milestone 1 host: SDL2 (WinRT backend) owns the CoreWindow, events and the gamepad.
// We render with our own D3D11 swapchain so its size can follow the real HDMI mode.
// Diagnostics: USB/storage access probe and HDMI mode probe (4K / 120 Hz), all in the log.

#include <windows.h>

#include <cstdint>
#include <string>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_syswm.h>

#include "DisplayProbe.h"
#include "Log.h"
#include "Renderer.h"
#include "StorageProbe.h"

namespace {

constexpr char kHostVersion[] = "milestone-1";

const float kColors[][4] = {
    {0.00f, 0.35f, 0.40f, 1.0f},  // teal (milestone 0 color)
    {0.10f, 0.45f, 0.10f, 1.0f},  // green
    {0.55f, 0.10f, 0.10f, 1.0f},  // red
    {0.10f, 0.15f, 0.55f, 1.0f},  // blue
    {0.60f, 0.45f, 0.05f, 1.0f},  // amber
};
const char* kColorNames[] = {"teal", "green", "red", "blue", "amber"};
constexpr int kNumColors = static_cast<int>(sizeof(kColors) / sizeof(kColors[0]));

void LogEnvironment() {
    RLOG("renpy-uwp host %s starting", kHostVersion);
    try {
        auto id = winrt::Windows::ApplicationModel::Package::Current().Id();
        RLOG("package: %ls", id.FullName().c_str());
    } catch (...) {
        RLOG("package: <unavailable>");
    }
    try {
        auto info = winrt::Windows::System::Profile::AnalyticsInfo::VersionInfo();
        uint64_t v = std::stoull(info.DeviceFamilyVersion().c_str());
        RLOG("device family: %ls, OS %llu.%llu.%llu.%llu", info.DeviceFamily().c_str(), (v >> 48) & 0xFFFF,
             (v >> 32) & 0xFFFF, (v >> 16) & 0xFFFF, v & 0xFFFF);
    } catch (...) {
        RLOG("device family: <unavailable>");
    }
    try {
        using winrt::Windows::System::MemoryManager;
        uint64_t limit = MemoryManager::AppMemoryUsageLimit();
        RLOG("memory: app limit %llu MB, usage %llu MB (about 1 GB = App type, about 5 GB = Game type)", limit >> 20,
             MemoryManager::AppMemoryUsage() >> 20);
    } catch (...) {
        RLOG("memory: <unavailable>");
    }
    SDL_version compiled, linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    RLOG("SDL compiled %u.%u.%u, linked %u.%u.%u (%s)", compiled.major, compiled.minor, compiled.patch, linked.major,
         linked.minor, linked.patch, SDL_GetRevision());
}

class Host {
public:
    int Run() {
        LogEnvironment();
        SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
            RLOG("fatal: SDL_Init failed: %s", SDL_GetError());
            return 1;
        }
        RLOG("SDL video driver: %s", SDL_GetCurrentVideoDriver());
        LogSdlDisplays();

        m_window = SDL_CreateWindow("Ren'Py UWP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1920, 1080, 0);
        if (!m_window) {
            RLOG("fatal: SDL_CreateWindow failed: %s", SDL_GetError());
            return 1;
        }
        int ww = 0, wh = 0, dw = 0, dh = 0;
        SDL_GetWindowSize(m_window, &ww, &wh);
        SDL_GL_GetDrawableSize(m_window, &dw, &dh);
        RLOG("SDL window: %dx%d (drawable %dx%d)", ww, wh, dw, dh);

        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(m_window, &info) || info.subsystem != SDL_SYSWM_WINRT) {
            RLOG("fatal: SDL_GetWindowWMInfo failed: %s", SDL_GetError());
            return 1;
        }
        auto* coreWindow = reinterpret_cast<IUnknown*>(info.info.winrt.window);
        try {
            winrt::Windows::UI::Core::CoreWindow cw{nullptr};
            winrt::copy_from_abi(cw, info.info.winrt.window);
            RLOG("CoreWindow: visible=%d, active flags SDL=0x%08x", cw.Visible() ? 1 : 0,
                 static_cast<unsigned>(SDL_GetWindowFlags(m_window)));
            cw.VisibilityChanged([](auto&&, winrt::Windows::UI::Core::VisibilityChangedEventArgs const& a) {
                RLOG("CoreWindow: VisibilityChanged visible=%d", a.Visible() ? 1 : 0);
            });
            cw.Activated([](auto&&, winrt::Windows::UI::Core::WindowActivatedEventArgs const& a) {
                RLOG("CoreWindow: Activated state=%d", static_cast<int>(a.WindowActivationState()));
            });
        } catch (...) {
            RLOG("CoreWindow: cannot query visibility");
        }

        // USB / storage probe runs in the background; press Y to run it again.
        renpyuwp::storage::RunAsync();

        // HDMI: log everything, start the swapchain at the current output size.
        bool haveHdmi = renpyuwp::display::Init();
        auto cur = renpyuwp::display::Current();
        uint32_t w = cur.width ? cur.width : 1920, h = cur.height ? cur.height : 1080;
        if (!m_renderer.Init(coreWindow, w, h)) {
            RLOG("fatal: renderer initialization failed");
            return 1;
        }
        Measure(3.0, "baseline (initial output mode)");

        if (haveHdmi) {
            auto best = renpyuwp::display::RequestBest([this] { PumpEvents(); });
            LogWindowState("after HDMI request");
            if (best.width && !m_renderer.Recreate(best.width, best.height)) {
                RLOG("fatal: swapchain recreation failed");
                return 1;
            }
            char label[96];
            snprintf(label, sizeof(label), "final output %ux%u @ %.3f Hz, swapchain %ux%u", best.width, best.height,
                     best.refreshHz, m_renderer.Width(), m_renderer.Height());
            Measure(5.0, label);
        }

        RLOG("entering main loop (A/B: color, X: fps report, Y: rerun storage probe)");
        m_meter.Start();
        m_lastPeriodicReport = GetTickCount64();
        ResetTimers();
        while (m_running) {
            TimedPump();
            if (!Frame()) break;
            if (GetTickCount64() - m_lastPeriodicReport > 30000) {
                ReportFps("periodic (last 30 s)");
                m_meter.Start();
                ResetTimers();
                m_lastPeriodicReport = GetTickCount64();
            }
        }
        RLOG("main loop finished");
        renpyuwp::display::RestoreDefault();
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        return 0;
    }

private:
    void LogWindowState(const char* when) {
        try {
            auto cw = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread();
            auto b = cw.Bounds();
            RLOG("window state (%s): CoreWindow visible=%d bounds %.0fx%.0f, SDL flags 0x%08x", when,
                 cw.Visible() ? 1 : 0, b.Width, b.Height, static_cast<unsigned>(SDL_GetWindowFlags(m_window)));
        } catch (...) {
            RLOG("window state (%s): unavailable", when);
        }
    }

    void LogSdlDisplays() {
        int n = SDL_GetNumVideoDisplays();
        for (int d = 0; d < n; ++d) {
            SDL_DisplayMode m;
            if (SDL_GetCurrentDisplayMode(d, &m) == 0) {
                RLOG("SDL display %d: current %dx%d @ %d Hz", d, m.w, m.h, m.refresh_rate);
            }
            int modes = SDL_GetNumDisplayModes(d);
            for (int i = 0; i < modes; ++i) {
                if (SDL_GetDisplayMode(d, i, &m) == 0) RLOG("SDL display %d mode %d: %dx%d @ %d Hz", d, i, m.w, m.h, m.refresh_rate);
            }
        }
    }

    bool Frame() {
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        bool ok = m_renderer.Frame(kColors[m_color]);
        QueryPerformanceCounter(&t1);
        m_presentTicks += t1.QuadPart - t0.QuadPart;
        if (!ok) return false;
        m_meter.Tick();
        return true;
    }

    // Times event pumping separately from Present, to tell vsync waits from SDL blocking.
    void TimedPump() {
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        PumpEvents();
        QueryPerformanceCounter(&t1);
        m_pumpTicks += t1.QuadPart - t0.QuadPart;
    }

    void ResetTimers() {
        m_pumpTicks = 0;
        m_presentTicks = 0;
    }

    void Measure(double seconds, const char* label) {
        RLOG("fps: measuring %.0f s: %s", seconds, label);
        uint32_t pc0 = 0, prc0 = 0, src0 = 0, pc1 = 0, prc1 = 0, src1 = 0;
        bool stats0 = m_renderer.FrameStatistics(pc0, prc0, src0);
        m_meter.Start();
        ResetTimers();
        while (m_running && m_meter.Seconds() < seconds) {
            TimedPump();
            if (!Frame()) break;
        }
        ReportFps(label);
        if (stats0 && m_renderer.FrameStatistics(pc1, prc1, src1)) {
            double s = m_meter.Seconds();
            RLOG("fps: DXGI stats: %u presents, %u vblanks over %.2f s -> display refresh about %.2f Hz", pc1 - pc0,
                 src1 - src0, s, s > 0 ? (src1 - src0) / s : 0.0);
        } else {
            RLOG("fps: DXGI frame statistics not available");
        }
    }

    void ReportFps(const char* label) {
        RLOG("fps: RESULT %s: %.2f fps over %.2f s (%llu frames), frame time min %.2f ms max %.2f ms, hitches %llu",
             label, m_meter.Fps(), m_meter.Seconds(), static_cast<unsigned long long>(m_meter.Frames()),
             m_meter.MinMs(), m_meter.MaxMs(), static_cast<unsigned long long>(m_meter.Hitches()));
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        double frames = m_meter.Frames() ? static_cast<double>(m_meter.Frames()) : 1.0;
        RLOG("fps:   avg per frame: event pump %.2f ms, Present %.2f ms",
             1000.0 * static_cast<double>(m_pumpTicks) / static_cast<double>(f.QuadPart) / frames,
             1000.0 * static_cast<double>(m_presentTicks) / static_cast<double>(f.QuadPart) / frames);
        uint32_t ok, occ, other;
        long last;
        m_renderer.TakePresentStats(ok, occ, other, last);
        RLOG("fps:   Present results: S_OK=%u OCCLUDED=%u other=%u (last other 0x%08lx)", ok, occ, other,
             static_cast<unsigned long>(last));
    }

    void PumpEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) HandleEvent(e);
    }

    void HandleEvent(const SDL_Event& e) {
        switch (e.type) {
            case SDL_QUIT:
                RLOG("event: SDL_QUIT");
                m_running = false;
                break;
            case SDL_APP_WILLENTERBACKGROUND:
                RLOG("event: will enter background");
                renpyuwp::display::RestoreDefault();
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                RLOG("event: did enter foreground");
                renpyuwp::display::Reapply();
                break;
            case SDL_CONTROLLERDEVICEADDED: {
                SDL_GameController* gc = SDL_GameControllerOpen(e.cdevice.which);
                RLOG("input: controller added #%d: %s", e.cdevice.which,
                     gc ? SDL_GameControllerName(gc) : SDL_GetError());
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
                RLOG("input: controller removed (instance %d)", e.cdevice.which);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                OnButton(static_cast<SDL_GameControllerButton>(e.cbutton.button));
                break;
            case SDL_KEYDOWN:
                RLOG("input: key down %s (scancode %d)", SDL_GetKeyName(e.key.keysym.sym), e.key.keysym.scancode);
                break;
            case SDL_WINDOWEVENT:
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        RLOG("event: window size changed to %dx%d", e.window.data1, e.window.data2);
                        break;
                    case SDL_WINDOWEVENT_SHOWN: RLOG("event: window shown"); break;
                    case SDL_WINDOWEVENT_HIDDEN: RLOG("event: window hidden"); break;
                    case SDL_WINDOWEVENT_EXPOSED: RLOG("event: window exposed"); break;
                    case SDL_WINDOWEVENT_MINIMIZED: RLOG("event: window minimized"); break;
                    case SDL_WINDOWEVENT_RESTORED: RLOG("event: window restored"); break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED: RLOG("event: focus gained"); break;
                    case SDL_WINDOWEVENT_FOCUS_LOST: RLOG("event: focus lost"); break;
                    default: break;
                }
                break;
            default:
                break;
        }
    }

    void OnButton(SDL_GameControllerButton b) {
        const char* name = SDL_GameControllerGetStringForButton(b);
        switch (b) {
            case SDL_CONTROLLER_BUTTON_A:
                m_color = (m_color + 1) % kNumColors;
                RLOG("input: button A -> color %s", kColorNames[m_color]);
                break;
            case SDL_CONTROLLER_BUTTON_B:
                m_color = (m_color + kNumColors - 1) % kNumColors;
                RLOG("input: button B -> color %s", kColorNames[m_color]);
                break;
            case SDL_CONTROLLER_BUTTON_X:
                RLOG("input: button X -> fps report");
                ReportFps("on request (since last report)");
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                RLOG("input: button Y -> storage probe");
                renpyuwp::storage::RunAsync();
                break;
            default:
                RLOG("input: button %s", name ? name : "?");
                break;
        }
    }

    SDL_Window* m_window = nullptr;
    renpyuwp::Renderer m_renderer;
    renpyuwp::FpsMeter m_meter;
    ULONGLONG m_lastPeriodicReport = 0;
    LONGLONG m_pumpTicks = 0;
    LONGLONG m_presentTicks = 0;
    int m_color = 0;
    bool m_running = true;
};

int SdlMain(int, char**) {
    Host host;
    int rc = host.Run();
    RLOG("host exiting with code %d", rc);
    return rc;
}

}  // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // CoreApplication::Run (inside SDL_WinRTRunApp) expects an MTA main thread, like milestone 0.
    winrt::init_apartment();
    renpyuwp::log::Init();
    RLOG("wWinMain: starting SDL_WinRTRunApp");
    int rc = SDL_WinRTRunApp(SdlMain, nullptr);
    RLOG("SDL_WinRTRunApp returned %d", rc);
    renpyuwp::log::Shutdown();
    return rc;
}
