// Milestone 0 host: proves the build -> sign -> deploy -> log chain.
// CoreApplication + D3D11 swapchain cleared to a fixed color, log in LocalState.
// SDL2 replaces the rendering/input part in milestone 1.

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <string>

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/Windows.UI.Core.h>

#include "Log.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::UI::Core;

namespace {

constexpr char kHostVersion[] = "milestone-0";

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
    IFrameworkView CreateView() { return *this; }

    void Initialize(CoreApplicationView const& view) {
        view.Activated({this, &App::OnActivated});
        CoreApplication::Suspending([](auto&&, auto&&) { RLOG("event: suspending"); });
        CoreApplication::Resuming([](auto&&, auto&&) { RLOG("event: resuming"); });
    }

    void SetWindow(CoreWindow const& window) {
        window.Closed([this](auto&&, auto&&) {
            RLOG("event: window closed");
            m_running = false;
        });
        m_window = window;
        auto bounds = window.Bounds();
        RLOG("window bounds: %.0fx%.0f (DIPs)", bounds.Width, bounds.Height);
    }

    void Load(hstring const&) {}

    void Run() {
        if (!CreateDevice()) {
            RLOG("fatal: D3D11 initialization failed, exiting");
            return;
        }
        RLOG("entering main loop");
        // Dark teal: distinct from the splash screen background so the user can tell it is running.
        const float clear[4] = {0.0f, 0.35f, 0.40f, 1.0f};
        unsigned frames = 0;
        while (m_running) {
            m_window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            ID3D11RenderTargetView* rtvs[] = {m_rtv.get()};
            m_context->OMSetRenderTargets(1, rtvs, nullptr);
            m_context->ClearRenderTargetView(m_rtv.get(), clear);
            HRESULT hr = m_swapchain->Present(1, 0);
            if (FAILED(hr)) {
                RLOG("Present failed: 0x%08lx", static_cast<unsigned long>(hr));
                break;
            }
            if (++frames == 120) {
                RLOG("rendered 120 frames OK");
            }
        }
        RLOG("main loop finished after %u frames", frames);
    }

    void Uninitialize() {}

private:
    void OnActivated(CoreApplicationView const&, IActivatedEventArgs const& args) {
        RLOG("event: activated (kind=%d)", static_cast<int>(args.Kind()));
        CoreWindow::GetForCurrentThread().Activate();
    }

    bool CreateDevice() {
        D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
        D3D_FEATURE_LEVEL got{};
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, m_device.put(), &got,
                                       m_context.put());
        if (FAILED(hr)) {
            RLOG("D3D11CreateDevice failed: 0x%08lx", static_cast<unsigned long>(hr));
            return false;
        }
        RLOG("D3D11 device created, feature level 0x%04x", static_cast<unsigned>(got));

        auto dxgiDevice = m_device.as<IDXGIDevice>();
        com_ptr<IDXGIAdapter> adapter;
        check_hresult(dxgiDevice->GetAdapter(adapter.put()));
        DXGI_ADAPTER_DESC adesc{};
        adapter->GetDesc(&adesc);
        RLOG("adapter: %ls, dedicated video memory %llu MB", adesc.Description,
             static_cast<unsigned long long>(adesc.DedicatedVideoMemory / (1024 * 1024)));
        com_ptr<IDXGIFactory2> factory;
        check_hresult(adapter->GetParent(IID_PPV_ARGS(factory.put())));

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = 0;  // use CoreWindow size (1080p on Xbox; 4K handled in a later milestone)
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.Scaling = DXGI_SCALING_STRETCH;
        hr = factory->CreateSwapChainForCoreWindow(m_device.get(), winrt::get_unknown(m_window), &sd, nullptr,
                                                   m_swapchain.put());
        if (FAILED(hr)) {
            RLOG("CreateSwapChainForCoreWindow failed: 0x%08lx", static_cast<unsigned long>(hr));
            return false;
        }
        m_swapchain->GetDesc1(&sd);
        RLOG("swapchain created: %ux%u", sd.Width, sd.Height);

        com_ptr<ID3D11Texture2D> backbuffer;
        check_hresult(m_swapchain->GetBuffer(0, IID_PPV_ARGS(backbuffer.put())));
        check_hresult(m_device->CreateRenderTargetView(backbuffer.get(), nullptr, m_rtv.put()));
        return true;
    }

    CoreWindow m_window{nullptr};
    bool m_running = true;
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11DeviceContext> m_context;
    com_ptr<IDXGISwapChain1> m_swapchain;
    com_ptr<ID3D11RenderTargetView> m_rtv;
};

void LogEnvironment() {
    RLOG("renpy-uwp host %s starting", kHostVersion);
    try {
        auto id = Package::Current().Id();
        auto v = id.Version();
        RLOG("package: %ls %u.%u.%u.%u", id.FullName().c_str(), v.Major, v.Minor, v.Build, v.Revision);
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
}

}  // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    winrt::init_apartment();
    renpyuwp::log::Init();
    LogEnvironment();
    CoreApplication::Run(make<App>());
    RLOG("CoreApplication::Run returned");
    renpyuwp::log::Shutdown();
    return 0;
}
