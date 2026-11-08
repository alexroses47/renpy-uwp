// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_3.h>

#include <cstdint>

#include <winrt/base.h>

namespace renpyuwp {

// D3D11 swapchain on the CoreWindow with an explicit size (independent of the
// 1920x1080 DIP size the CoreWindow reports on Xbox).
class Renderer {
public:
    bool Init(IUnknown* coreWindow, uint32_t width, uint32_t height);
    bool Resize(uint32_t width, uint32_t height);
    // Destroys and creates the swapchain again (needed after an HDMI mode change). If the D3D device
    // was removed (GPU reset), logs the reason and creates a new device first.
    bool Recreate(uint32_t width, uint32_t height);
    // S_OK if the device is healthy, otherwise the DXGI removal reason.
    long DeviceRemovedReason() const;
    enum class FrameResult { Presented, Skipped, Error };
    // Waits (at most `waitMs`) for the swapchain to accept a frame, then clears and presents with vsync.
    // Skipped = the display did not release a buffer in time (no vsync), so nothing was presented.
    FrameResult Frame(const float rgba[4], DWORD waitMs = 100);
    // Present count / refresh count from DXGI frame statistics, if available.
    bool FrameStatistics(uint32_t& presentCount, uint32_t& presentRefreshCount, uint32_t& syncRefreshCount);

    // Present() results since the last call: S_OK count, DXGI_STATUS_OCCLUDED count, other count/last code.
    void TakePresentStats(uint32_t& ok, uint32_t& occluded, uint32_t& other, long& lastOther, uint32_t& skipped);

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

private:
    bool CreateDevice();
    void ReleaseAll();
    bool CreateTargets();
    bool CreateSwapchain(uint32_t width, uint32_t height);

    winrt::com_ptr<ID3D11Device> m_device;
    winrt::com_ptr<ID3D11DeviceContext> m_context;
    winrt::com_ptr<IDXGISwapChain1> m_swapchain;
    HANDLE m_frameWait = nullptr;
    uint32_t m_skipped = 0;
    winrt::com_ptr<ID3D11RenderTargetView> m_rtv;
    winrt::com_ptr<IDXGIFactory2> m_factory;
    IUnknown* m_coreWindow = nullptr;
    uint32_t m_ok = 0, m_occluded = 0, m_other = 0;
    long m_lastOther = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

// Frame timing over a measurement window (QueryPerformanceCounter based).
class FpsMeter {
public:
    void Start();
    void Tick();
    double Seconds() const;
    uint64_t Frames() const { return m_frames; }
    double Fps() const;
    double MinMs() const { return m_minMs; }
    double MaxMs() const { return m_maxMs; }
    // Frames whose interval exceeded 1.5x the average (hitches).
    uint64_t Hitches() const;

private:
    LARGE_INTEGER m_freq{}, m_start{}, m_last{};
    uint64_t m_frames = 0;
    double m_minMs = 1e9, m_maxMs = 0;
    static constexpr int kMaxSamples = 4096;
    float m_samples[kMaxSamples]{};
    int m_count = 0;
};

}  // namespace renpyuwp
