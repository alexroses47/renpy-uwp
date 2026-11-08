// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Renderer.h"

#include "Log.h"

namespace renpyuwp {

bool Renderer::Init(IUnknown* coreWindow, uint32_t width, uint32_t height) {
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                                   ARRAYSIZE(levels), D3D11_SDK_VERSION, m_device.put(), &got, m_context.put());
    if (FAILED(hr)) {
        RLOG("D3D11CreateDevice failed: 0x%08lx", static_cast<unsigned long>(hr));
        return false;
    }
    RLOG("D3D11 device created, feature level 0x%04x", static_cast<unsigned>(got));

    auto dxgiDevice = m_device.as<IDXGIDevice>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(dxgiDevice->GetAdapter(adapter.put()));
    DXGI_ADAPTER_DESC adesc{};
    adapter->GetDesc(&adesc);
    RLOG("adapter: %ls, dedicated video memory %llu MB, shared %llu MB", adesc.Description,
         static_cast<unsigned long long>(adesc.DedicatedVideoMemory >> 20),
         static_cast<unsigned long long>(adesc.SharedSystemMemory >> 20));
    winrt::check_hresult(adapter->GetParent(IID_PPV_ARGS(m_factory.put())));
    m_coreWindow = coreWindow;
    return CreateSwapchain(width, height);
}

bool Renderer::CreateSwapchain(uint32_t width, uint32_t height) {
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = width;
    sd.Height = height;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Scaling = DXGI_SCALING_STRETCH;
    // Waitable swapchain: the render loop never blocks inside Present, so input keeps flowing
    // even if the display stops producing vsyncs (seen at 3840x2160 @ 120 Hz).
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    HRESULT hr = m_factory->CreateSwapChainForCoreWindow(m_device.get(), m_coreWindow, &sd, nullptr, m_swapchain.put());
    if (FAILED(hr)) {
        RLOG("CreateSwapChainForCoreWindow(%ux%u) failed: 0x%08lx", width, height, static_cast<unsigned long>(hr));
        return false;
    }
    auto sc2 = m_swapchain.as<IDXGISwapChain2>();
    sc2->SetMaximumFrameLatency(1);
    m_frameWait = sc2->GetFrameLatencyWaitableObject();
    return CreateTargets();
}

bool Renderer::Resize(uint32_t width, uint32_t height) {
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv = nullptr;
    m_context->Flush();
    HRESULT hr = m_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
                                            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
    if (FAILED(hr)) {
        RLOG("ResizeBuffers(%ux%u) failed: 0x%08lx", width, height, static_cast<unsigned long>(hr));
        return false;
    }
    return CreateTargets();
}

bool Renderer::Recreate(uint32_t width, uint32_t height) {
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv = nullptr;
    if (m_frameWait) {
        CloseHandle(m_frameWait);
        m_frameWait = nullptr;
    }
    m_swapchain = nullptr;
    m_context->ClearState();
    m_context->Flush();
    RLOG("recreating swapchain at %ux%u", width, height);
    return CreateSwapchain(width, height);
}

void Renderer::TakePresentStats(uint32_t& ok, uint32_t& occluded, uint32_t& other, long& lastOther,
                                uint32_t& skipped) {
    skipped = m_skipped;
    m_skipped = 0;
    ok = m_ok;
    occluded = m_occluded;
    other = m_other;
    lastOther = m_lastOther;
    m_ok = m_occluded = m_other = 0;
    m_lastOther = 0;
}

bool Renderer::CreateTargets() {
    DXGI_SWAP_CHAIN_DESC1 sd{};
    m_swapchain->GetDesc1(&sd);
    m_width = sd.Width;
    m_height = sd.Height;
    winrt::com_ptr<ID3D11Texture2D> backbuffer;
    HRESULT hr = m_swapchain->GetBuffer(0, IID_PPV_ARGS(backbuffer.put()));
    if (SUCCEEDED(hr)) {
        hr = m_device->CreateRenderTargetView(backbuffer.get(), nullptr, m_rtv.put());
    }
    if (FAILED(hr)) {
        RLOG("render target creation failed: 0x%08lx", static_cast<unsigned long>(hr));
        return false;
    }
    RLOG("swapchain ready: %ux%u", m_width, m_height);
    return true;
}

Renderer::FrameResult Renderer::Frame(const float rgba[4], DWORD waitMs) {
    if (m_frameWait && WaitForSingleObjectEx(m_frameWait, waitMs, TRUE) != WAIT_OBJECT_0) {
        ++m_skipped;
        return FrameResult::Skipped;
    }
    ID3D11RenderTargetView* rtvs[] = {m_rtv.get()};
    m_context->OMSetRenderTargets(1, rtvs, nullptr);
    m_context->ClearRenderTargetView(m_rtv.get(), rgba);
    HRESULT hr = m_swapchain->Present(1, 0);
    if (hr == S_OK) {
        ++m_ok;
    } else if (hr == DXGI_STATUS_OCCLUDED) {
        ++m_occluded;
    } else {
        ++m_other;
        m_lastOther = hr;
    }
    if (FAILED(hr)) {
        RLOG("Present failed: 0x%08lx", static_cast<unsigned long>(hr));
        return FrameResult::Error;
    }
    return FrameResult::Presented;
}

bool Renderer::FrameStatistics(uint32_t& presentCount, uint32_t& presentRefreshCount, uint32_t& syncRefreshCount) {
    DXGI_FRAME_STATISTICS st{};
    if (FAILED(m_swapchain->GetFrameStatistics(&st))) {
        return false;
    }
    presentCount = st.PresentCount;
    presentRefreshCount = st.PresentRefreshCount;
    syncRefreshCount = st.SyncRefreshCount;
    return true;
}

void FpsMeter::Start() {
    QueryPerformanceFrequency(&m_freq);
    QueryPerformanceCounter(&m_start);
    m_last = m_start;
    m_frames = 0;
    m_minMs = 1e9;
    m_maxMs = 0;
    m_count = 0;
}

void FpsMeter::Tick() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double ms = 1000.0 * static_cast<double>(now.QuadPart - m_last.QuadPart) / static_cast<double>(m_freq.QuadPart);
    m_last = now;
    ++m_frames;
    if (ms < m_minMs) m_minMs = ms;
    if (ms > m_maxMs) m_maxMs = ms;
    if (m_count < kMaxSamples) m_samples[m_count++] = static_cast<float>(ms);
}

double FpsMeter::Seconds() const {
    return static_cast<double>(m_last.QuadPart - m_start.QuadPart) / static_cast<double>(m_freq.QuadPart);
}

double FpsMeter::Fps() const {
    double s = Seconds();
    return s > 0 ? static_cast<double>(m_frames) / s : 0.0;
}

uint64_t FpsMeter::Hitches() const {
    if (m_frames == 0) return 0;
    double avg = 1000.0 * Seconds() / static_cast<double>(m_frames);
    uint64_t n = 0;
    for (int i = 0; i < m_count; ++i) {
        if (m_samples[i] > 1.5 * avg) ++n;
    }
    return n;
}

}  // namespace renpyuwp
