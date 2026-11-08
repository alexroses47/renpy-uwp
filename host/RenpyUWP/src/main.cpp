// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Milestone 1 host: SDL2 (WinRT backend) owns the CoreWindow, events and the gamepad.
// We render with our own D3D11 swapchain so its size can follow the real HDMI mode.
// Diagnostics: USB/storage access probe and HDMI mode probe (4K / 120 Hz), all in the log.

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>

#include <winrt/Windows.ApplicationModel.Core.h>
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
        m_haveHdmi = haveHdmi;
        InstallLifecycleHandlers();
        Measure(3.0, "baseline (initial output mode)");

        if (haveHdmi && !ApplyPolicy()) return 1;

        RLOG("entering main loop (A/B: color, X: fps+vram report, Y: storage probe, View: system HDMI mode, "
             "Menu: re-apply 4K120/1440p, RB: diagnostic mode matrix)");
        m_meter.Start();
        m_lastPeriodicReport = GetTickCount64();
        ResetTimers();
        m_lastPresent = GetTickCount64();
        while (m_running) {
            TimedPump();
            if (!RunPendingAction()) break;
            auto r = Frame();
            if (r == renpyuwp::Renderer::FrameResult::Presented) m_lastPresent = GetTickCount64();
            if (!Watchdog(r)) break;
            if (GetTickCount64() - m_lastPeriodicReport > 30000) {
                ReportFps("periodic (last 30 s)");
                m_renderer.LogVideoMemory("periodic");
                m_meter.Start();
                ResetTimers();
                m_lastPeriodicReport = GetTickCount64();
            }
        }
        RLOG("main loop finished; restoring the system display mode");
        renpyuwp::display::RestoreDefault();
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        return 0;
    }

private:
    // ---- Default output policy (user decision 2026-11-08): HDMI 3840x2160 @ 119.88 Hz with a
    // 2560x1440 swapchain (60 fps: UWP composition limit). Falls back to 1920x1080 @ 60 Hz on any
    // device loss or stall, and logs why.
    static constexpr int kPolicyCandidate = 0;    // 3840x2160 @ 120 Hz
    static constexpr int kFallbackCandidate = 4;  // 1920x1080 @ 60 Hz
    static constexpr uint32_t kPolicyW = 2560, kPolicyH = 1440;
    static constexpr double kMinFps = 50.0;

    bool ApplyPolicy() {
        RLOG("policy: applying HDMI %s with swapchain %ux%u", renpyuwp::display::CandidateLabel(kPolicyCandidate),
             kPolicyW, kPolicyH);
        uint32_t losses = m_renderer.DeviceLosses();
        m_renderer.LogVideoMemory("before mode change");
        m_renderer.ReleaseSwapchain();  // no swapchain alive while the output mode changes
        renpyuwp::display::Mode active;
        if (!renpyuwp::display::RequestCandidate(kPolicyCandidate, [this] { PumpEvents(); }, active)) {
            return Fallback("4K120 mode could not be set");
        }
        renpyuwp::display::Mode sc{kPolicyW, kPolicyH, active.refreshHz};
        double fps = 0;
        if (!RebuildAndMeasure(sc, "policy output 3840x2160", &fps)) return false;
        if (m_renderer.DeviceLosses() != losses || m_renderer.DeviceRemovedReason() != 0) {
            return Fallback("D3D device lost while switching to 4K120/1440p");
        }
        if (fps < kMinFps) {
            char why[96];
            snprintf(why, sizeof(why), "only %.1f fps after switching to 4K120/1440p", fps);
            return Fallback(why);
        }
        m_policyActive = true;
        m_inFallback = false;
        RLOG("policy: ACTIVE, output %ux%u @ %.3f Hz, swapchain %ux%u, %.1f fps", active.width, active.height,
             active.refreshHz, m_renderer.Width(), m_renderer.Height(), fps);
        return true;
    }

    bool Fallback(const char* reason) {
        RLOG("policy: FALLBACK to 1920x1080 @ 60 Hz: %s", reason);
        m_policyActive = false;
        m_inFallback = true;
        m_renderer.ReleaseSwapchain();
        renpyuwp::display::Mode active;
        if (!renpyuwp::display::RequestCandidate(kFallbackCandidate, [this] { PumpEvents(); }, active)) {
            RLOG("policy: 1080p60 not settable; restoring the system default mode instead");
            renpyuwp::display::RestoreDefault();
            Wait(3.0);
            active = renpyuwp::display::Current();
        }
        renpyuwp::display::Mode sc{1920, 1080, active.refreshHz};
        double fps = 0;
        if (!RebuildAndMeasure(sc, "fallback output", &fps)) return false;
        RLOG("policy: fallback active, output %ux%u @ %.3f Hz, %.1f fps", active.width, active.height,
             active.refreshHz, fps);
        return true;
    }

    // Main-loop watchdog: device loss or 3 s without a presented frame -> fallback.
    bool Watchdog(renpyuwp::Renderer::FrameResult r) {
        bool lost = m_renderer.DeviceRemovedReason() != 0;
        bool stalled = GetTickCount64() - m_lastPresent > 3000;
        if (!lost && !stalled && r != renpyuwp::Renderer::FrameResult::Error) return true;
        RLOG("watchdog: %s (Present result %d, device removed reason 0x%08lx)",
             lost ? "D3D device lost" : (stalled ? "no frame presented for 3 s" : "Present error"), static_cast<int>(r),
             static_cast<unsigned long>(m_renderer.DeviceRemovedReason()));
        if (m_inFallback && !lost) {
            // Already at 1080p60: just rebuild the swapchain once more.
            bool ok = m_renderer.Recreate(1920, 1080);
            m_lastPresent = GetTickCount64();
            return ok;
        }
        bool ok = Fallback("watchdog in main loop");
        m_lastPresent = GetTickCount64();
        return ok;
    }

    void Wait(double seconds) {
        ULONGLONG until = GetTickCount64() + static_cast<ULONGLONG>(seconds * 1000);
        while (GetTickCount64() < until) {
            PumpEvents();
            Sleep(10);
        }
    }

    // Restore the system display mode whenever the app leaves the foreground, re-apply on resume.
    // Registered directly on CoreApplication (UI thread) so it runs even if the main loop is busy.
    void InstallLifecycleHandlers() {
        using winrt::Windows::ApplicationModel::Core::CoreApplication;
        CoreApplication::Suspending([this](auto&&, auto&&) {
            RLOG("lifecycle: suspending -> restoring the system display mode");
            renpyuwp::display::RestoreDefault();
            m_suspendedWithPolicy = m_policyActive || m_inFallback;
        });
        CoreApplication::Resuming([this](auto&&, auto&&) {
            RLOG("lifecycle: resuming -> will re-apply the output policy");
            if (m_suspendedWithPolicy) m_resumePending = true;
        });
        CoreApplication::Exiting([](auto&&, auto&&) {
            RLOG("lifecycle: exiting -> restoring the system display mode");
            renpyuwp::display::RestoreDefault();
        });
    }

    // Test matrix: HDMI mode x swapchain size. Separates "the output mode does not run" from
    // "this swapchain size cannot be scanned out at this rate". Riskiest trial (4K swapchain at
    // 120 Hz, which removed the D3D device in build 0.0.12) goes last. The best working
    // combination is applied at the end. Candidate indexes: see DisplayProbe (0=4K120, 2=4K60).
    struct Trial {
        int candidate;
        uint32_t sw, sh;
        UINT sync = 1;  // 0 = no vsync: diagnostic only, never selected as best
        double hz = 0, fps = 0;
        bool set = false;
    };

    bool RunTrial(Trial& t) {
        auto pump = [this] { PumpEvents(); };
        renpyuwp::display::Mode active;
        t.set = renpyuwp::display::RequestCandidate(t.candidate, pump, active);
        if (!t.set) return true;
        t.hz = active.refreshHz > 0 ? active.refreshHz : 60.0;
        renpyuwp::display::Mode sc{t.sw, t.sh, active.refreshHz};
        char what[96];
        snprintf(what, sizeof(what), "trial output %ux%u", active.width, active.height);
        m_syncInterval = t.sync;
        bool ok = RebuildAndMeasure(sc, what, &t.fps);
        m_syncInterval = 1;
        if (!ok) return false;
        RLOG("HDMI: TRIAL %s, swapchain %ux%u, Present(%u) -> %.1f fps (%.0f%% of %.3f Hz)%s",
             renpyuwp::display::CandidateLabel(t.candidate), t.sw, t.sh, t.sync, t.fps, 100.0 * t.fps / t.hz, t.hz,
             m_renderer.DeviceRemovedReason() ? " [D3D DEVICE REMOVED]" : "");
        return true;
    }

    bool TryBestMode() {
        // 3840x2160 swapchains are left out: in build 0.0.13 they reset the GPU (DXGI_ERROR_DEVICE_RESET)
        // at both 60 and 120 Hz. Larger-than-1440p sizes go last in each group.
        Trial trials[] = {
            {2, 2560, 1440},                                    // 4K60 output, control
            {0, 2560, 1440}, {0, 2560, 1440, 0},                // 4K120: vsync vs no vsync
            {3, 1920, 1080}, {3, 1920, 1080, 0},                // 1080p120 output
            {2, 2880, 1620}, {2, 3200, 1800},                   // 4K60: largest swapchain that works?
        };
        for (auto& t : trials) {
            if (!RunTrial(t)) return false;
        }
        RLOG("HDMI: SUMMARY (output mode / swapchain / fps):");
        Trial* best = nullptr;
        auto score = [](const Trial& t) {
            bool full = t.fps >= 0.9 * t.hz;
            return (full ? 1e9 : 0) + t.hz * 1e6 + static_cast<double>(t.sw) * t.sh;
        };
        for (auto& t : trials) {
            bool ok = t.set && t.fps >= 0.9 * t.hz;
            RLOG("HDMI:   %-28s swapchain %ux%u Present(%u): %s", renpyuwp::display::CandidateLabel(t.candidate), t.sw,
                 t.sh, t.sync, !t.set ? "mode not settable" : (ok ? "OK full rate" : "below refresh rate"));
            if (t.set) RLOG("HDMI:     %.1f fps of %.3f Hz", t.fps, t.hz);
            if (ok && t.sync == 1 && (!best || score(t) > score(*best))) best = &t;
        }
        if (!best) {
            RLOG("HDMI: no combination runs at full rate; going back to the default mode");
            return RestoreDefaultAndMeasure();
        }
        RLOG("HDMI: BEST = %s with swapchain %ux%u; applying it", renpyuwp::display::CandidateLabel(best->candidate),
             best->sw, best->sh);
        Trial final = *best;
        if (!RunTrial(final)) return false;
        if (!(final.set && final.fps >= 0.9 * final.hz)) {
            RLOG("HDMI: best combination did not reproduce; going back to the default mode");
            return RestoreDefaultAndMeasure();
        }
        RLOG("HDMI: FINAL %s, swapchain %ux%u, %.1f fps", renpyuwp::display::CandidateLabel(final.candidate), final.sw,
             final.sh, final.fps);
        m_activeMode = renpyuwp::display::Current();
        return true;
    }

    bool RestoreDefaultAndMeasure() {
        renpyuwp::display::RestoreDefault();
        Wait(3.0);
        m_activeMode = renpyuwp::display::Current();
        return RebuildAndMeasure(m_activeMode, "default output", nullptr);
    }

    bool RebuildAndMeasure(const renpyuwp::display::Mode& mode, const char* what, double* fpsOut) {
        uint32_t w = mode.width ? mode.width : 1920, h = mode.height ? mode.height : 1080;
        if (!m_renderer.Recreate(w, h)) {
            RLOG("fatal: swapchain recreation failed");
            return false;
        }
        Settle(2.0);
        char label[128];
        snprintf(label, sizeof(label), "%s %ux%u @ %.3f Hz, swapchain %ux%u", what, mode.width, mode.height,
                 mode.refreshHz, m_renderer.Width(), m_renderer.Height());
        double fps = Measure(4.0, label);
        if (fpsOut) *fpsOut = fps;
        m_meter.Start();
        ResetTimers();
        m_lastPeriodicReport = GetTickCount64();
        return true;
    }

    // Button actions that need their own wait/measure loops run here, outside event handling.
    bool RunPendingAction() {
        int action = m_pendingAction.exchange(0);
        if (m_resumePending) {
            m_resumePending = false;
            action = 2;
        }
        if (action == 1) {
            RLOG("action: restore the system HDMI mode");
            m_policyActive = false;
            m_inFallback = false;
            m_renderer.ReleaseSwapchain();
            return RestoreDefaultAndMeasure();
        }
        if (action == 2) {
            RLOG("action: re-apply output policy");
            return ApplyPolicy();
        }
        if (action == 3) {
            RLOG("action: diagnostic mode matrix (then re-apply the policy)");
            m_policyActive = false;
            return TryBestMode() && ApplyPolicy();
        }
        return true;
    }

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

    renpyuwp::Renderer::FrameResult Frame() {
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        auto r = m_renderer.Frame(kColors[m_color], 100, m_syncInterval);
        QueryPerformanceCounter(&t1);
        m_presentTicks += t1.QuadPart - t0.QuadPart;
        if (r == renpyuwp::Renderer::FrameResult::Presented) m_meter.Tick();
        return r;
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

    // Renders for `seconds` without measuring (lets the TV and the compositor settle after a mode change).
    void Settle(double seconds) {
        ULONGLONG until = GetTickCount64() + static_cast<ULONGLONG>(seconds * 1000);
        while (m_running && GetTickCount64() < until) {
            PumpEvents();
            if (Frame() == renpyuwp::Renderer::FrameResult::Error) break;
        }
    }

    // Measures presented frames per second over `seconds` of wall time. Returns the fps.
    double Measure(double seconds, const char* label) {
        RLOG("fps: measuring %.0f s: %s", seconds, label);
        uint32_t pc0 = 0, prc0 = 0, src0 = 0, pc1 = 0, prc1 = 0, src1 = 0;
        bool stats0 = m_renderer.FrameStatistics(pc0, prc0, src0);
        m_meter.Start();
        ResetTimers();
        ULONGLONG until = GetTickCount64() + static_cast<ULONGLONG>(seconds * 1000);
        while (m_running && GetTickCount64() < until) {
            TimedPump();
            if (Frame() == renpyuwp::Renderer::FrameResult::Error) break;
        }
        double wall = seconds;
        double fps = static_cast<double>(m_meter.Frames()) / wall;
        RLOG("fps: %llu frames presented in %.1f s of wall time = %.2f fps",
             static_cast<unsigned long long>(m_meter.Frames()), wall, fps);
        ReportFps(label);
        if (stats0 && m_renderer.FrameStatistics(pc1, prc1, src1)) {
            double s = m_meter.Seconds();
            RLOG("fps: DXGI stats: %u presents, %u vblanks over %.2f s -> display refresh about %.2f Hz", pc1 - pc0,
                 src1 - src0, s, s > 0 ? (src1 - src0) / s : 0.0);
        } else {
            RLOG("fps: DXGI frame statistics not available");
        }
        return fps;
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
        uint32_t ok, occ, other, skipped;
        long last;
        m_renderer.TakePresentStats(ok, occ, other, last, skipped);
        RLOG("fps:   Present results: S_OK=%u OCCLUDED=%u other=%u (last other 0x%08lx), skipped (no buffer within 100 ms)=%u",
             ok, occ, other, static_cast<unsigned long>(last), skipped);
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
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                RLOG("event: did enter foreground");
                break;
            case SDL_APP_TERMINATING:
                RLOG("event: terminating -> restoring the system display mode");
                renpyuwp::display::RestoreDefault();
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
                RLOG("input: button X -> fps and vram report");
                ReportFps("on request (since last report)");
                m_renderer.LogVideoMemory("on request");
                break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                RLOG("input: button RB -> diagnostic mode matrix");
                if (m_haveHdmi) m_pendingAction = 3;
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                RLOG("input: button Y -> storage probe");
                renpyuwp::storage::RunAsync();
                break;
            case SDL_CONTROLLER_BUTTON_BACK:
                RLOG("input: button View -> system HDMI mode");
                if (m_haveHdmi) m_pendingAction = 1;
                break;
            case SDL_CONTROLLER_BUTTON_START:
                RLOG("input: button Menu -> re-apply 4K120/1440p policy");
                if (m_haveHdmi) m_pendingAction = 2;
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
    std::atomic<int> m_pendingAction{0};
    std::atomic<bool> m_resumePending{false};
    std::atomic<bool> m_suspendedWithPolicy{false};
    bool m_policyActive = false;
    bool m_inFallback = false;
    ULONGLONG m_lastPresent = 0;
    UINT m_syncInterval = 1;
    renpyuwp::display::Mode m_activeMode;
    bool m_haveHdmi = false;
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
