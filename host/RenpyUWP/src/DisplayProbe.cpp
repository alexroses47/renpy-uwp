// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "DisplayProbe.h"

#include <windows.h>

#include <cmath>
#include <optional>
#include <string>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.Core.h>

#include "Log.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display::Core;

namespace renpyuwp::display {

namespace {

HdmiDisplayInformation g_hdmi{nullptr};
HdmiDisplayMode g_chosen{nullptr};

const char* ColorSpaceName(HdmiDisplayColorSpace c) {
    switch (c) {
        case HdmiDisplayColorSpace::RgbLimited: return "RgbLimited";
        case HdmiDisplayColorSpace::RgbFull: return "RgbFull";
        case HdmiDisplayColorSpace::BT2020: return "BT2020";
        case HdmiDisplayColorSpace::BT709: return "BT709";
        default: return "?";
    }
}

const char* EncodingName(HdmiDisplayPixelEncoding e) {
    switch (e) {
        case HdmiDisplayPixelEncoding::Rgb444: return "RGB444";
        case HdmiDisplayPixelEncoding::Ycc444: return "YCC444";
        case HdmiDisplayPixelEncoding::Ycc422: return "YCC422";
        case HdmiDisplayPixelEncoding::Ycc420: return "YCC420";
        default: return "?";
    }
}

std::string Describe(HdmiDisplayMode const& m) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%ux%u @ %.3f Hz, %u bpp, %s, %s, stereo=%d, hdr10=%d, 2086=%d, sdrLum=%d, dvLL=%d",
             m.ResolutionWidthInRawPixels(), m.ResolutionHeightInRawPixels(), m.RefreshRate(), m.BitsPerPixel(),
             ColorSpaceName(m.ColorSpace()), EncodingName(m.PixelEncoding()), m.StereoEnabled() ? 1 : 0,
             m.IsSmpte2084Supported() ? 1 : 0, m.Is2086MetadataSupported() ? 1 : 0,
             m.IsSdrLuminanceSupported() ? 1 : 0, m.IsDolbyVisionLowLatencySupported() ? 1 : 0);
    return buf;
}

// Higher is better for an SDR desktop-style output.
int SdrScore(HdmiDisplayMode const& m) {
    int s = 0;
    if (!m.StereoEnabled()) s += 100;
    if (m.ColorSpace() != HdmiDisplayColorSpace::BT2020) s += 50;
    if (m.PixelEncoding() == HdmiDisplayPixelEncoding::Rgb444) s += 20;
    if (m.BitsPerPixel() == 24) s += 10;
    return s;
}

std::optional<HdmiDisplayMode> Find(uint32_t w, uint32_t h, double minHz, double maxHz) {
    std::optional<HdmiDisplayMode> best;
    for (auto const& m : g_hdmi.GetSupportedDisplayModes()) {
        if (m.ResolutionWidthInRawPixels() != w || m.ResolutionHeightInRawPixels() != h) continue;
        double hz = m.RefreshRate();
        if (hz < minHz || hz > maxHz) continue;
        if (!best || SdrScore(m) > SdrScore(*best) ||
            (SdrScore(m) == SdrScore(*best) && hz > best->RefreshRate())) {
            best = m;
        }
    }
    return best;
}

// variant 0: (mode, HdrOption::None) as Kodi does for SDR; variant 1: (mode) overload.
bool RequestVariant(HdmiDisplayMode const& mode, int variant, const std::function<void()>& pump) {
    RLOG("HDMI: requesting %s [%s]", Describe(mode).c_str(), variant == 0 ? "hdrOption=None" : "no hdrOption");
    try {
        auto op = variant == 0 ? g_hdmi.RequestSetCurrentDisplayModeAsync(mode, HdmiDisplayHdrOption::None)
                               : g_hdmi.RequestSetCurrentDisplayModeAsync(mode);
        ULONGLONG deadline = GetTickCount64() + 15000;
        while (op.Status() == AsyncStatus::Started && GetTickCount64() < deadline) {
            pump();
            Sleep(10);
        }
        auto status = op.Status();
        if (status == AsyncStatus::Completed) {
            bool ok = op.GetResults();
            RLOG("HDMI: request completed, result=%s", ok ? "true" : "false");
            return ok;
        }
        if (status == AsyncStatus::Started) {
            RLOG("HDMI: request timed out after 15 s");
            op.Cancel();
            return false;
        }
        RLOG("HDMI: request ended with status %d, error 0x%08lx", static_cast<int>(status),
             static_cast<unsigned long>(op.ErrorCode()));
    } catch (hresult_error const& e) {
        RLOG("HDMI: request threw 0x%08lx: %ls", static_cast<unsigned long>(e.code()), e.message().c_str());
    }
    return false;
}

bool Request(HdmiDisplayMode const& mode, const std::function<void()>& pump) {
    return RequestVariant(mode, 0, pump) || RequestVariant(mode, 1, pump);
}

}  // namespace

bool Init() {
    try {
        g_hdmi = HdmiDisplayInformation::GetForCurrentView();
    } catch (hresult_error const& e) {
        RLOG("HDMI: GetForCurrentView threw 0x%08lx: %ls", static_cast<unsigned long>(e.code()), e.message().c_str());
        return false;
    }
    if (!g_hdmi) {
        RLOG("HDMI: HdmiDisplayInformation not available on this device");
        return false;
    }
    g_hdmi.DisplayModesChanged([](auto&&, auto&&) {
        auto m = g_hdmi.GetCurrentDisplayMode();
        RLOG("HDMI: event DisplayModesChanged, current %s", m ? Describe(m).c_str() : "<none>");
    });
    auto cur = g_hdmi.GetCurrentDisplayMode();
    RLOG("HDMI: current mode: %s", cur ? Describe(cur).c_str() : "<none>");
    auto modes = g_hdmi.GetSupportedDisplayModes();
    RLOG("HDMI: %u supported modes:", modes.Size());
    uint32_t i = 0;
    for (auto const& m : modes) {
        RLOG("HDMI:   [%2u] %s", i++, Describe(m).c_str());
    }
    return true;
}

Mode Current() {
    Mode r;
    if (!g_hdmi) return r;
    auto m = g_hdmi.GetCurrentDisplayMode();
    if (m) {
        r.width = m.ResolutionWidthInRawPixels();
        r.height = m.ResolutionHeightInRawPixels();
        r.refreshHz = m.RefreshRate();
    }
    return r;
}

namespace {
struct Candidate {
    uint32_t w, h;
    double minHz, maxHz;
    const char* label;
};
const Candidate kCandidates[] = {
    {3840, 2160, 119.0, 121.0, "3840x2160 @ 120 Hz (target)"},
    {2560, 1440, 119.0, 121.0, "2560x1440 @ 120 Hz"},
    {3840, 2160, 59.0, 61.0, "3840x2160 @ 60 Hz"},
    {1920, 1080, 119.0, 121.0, "1920x1080 @ 120 Hz"},
    {1920, 1080, 59.0, 61.0, "1920x1080 @ 60 Hz (fallback)"},
};
}  // namespace

int CandidateCount() { return static_cast<int>(sizeof(kCandidates) / sizeof(kCandidates[0])); }

const char* CandidateLabel(int index) { return kCandidates[index].label; }

bool RequestCandidate(int index, const std::function<void()>& pump, Mode& active) {
    if (!g_hdmi) return false;
    auto const& c = kCandidates[index];
    auto mode = Find(c.w, c.h, c.minHz, c.maxHz);
    if (!mode) {
        RLOG("HDMI: candidate %s: NOT in the supported mode list", c.label);
        return false;
    }
    RLOG("HDMI: candidate %s: listed", c.label);
    if (!Request(*mode, pump)) return false;
    // Give the TV a moment to resync, then report what is really active.
    ULONGLONG until = GetTickCount64() + 1500;
    while (GetTickCount64() < until) {
        pump();
        Sleep(10);
    }
    auto cur = g_hdmi.GetCurrentDisplayMode();
    RLOG("HDMI: active after request: %s", cur ? Describe(cur).c_str() : "<none>");
    g_chosen = *mode;
    active = Current();
    return true;
}

void Reapply() {
    if (!g_hdmi || !g_chosen) return;
    try {
        g_hdmi.RequestSetCurrentDisplayModeAsync(g_chosen, HdmiDisplayHdrOption::None);
        RLOG("HDMI: re-requested %s", Describe(g_chosen).c_str());
    } catch (hresult_error const& e) {
        RLOG("HDMI: reapply threw 0x%08lx", static_cast<unsigned long>(e.code()));
    }
}

void RestoreDefault() {
    if (!g_hdmi) return;
    try {
        g_chosen = nullptr;
        g_hdmi.SetDefaultDisplayModeAsync();
        RLOG("HDMI: requested default display mode");
    } catch (hresult_error const& e) {
        RLOG("HDMI: SetDefaultDisplayModeAsync threw 0x%08lx", static_cast<unsigned long>(e.code()));
    }
}

}  // namespace renpyuwp::display
