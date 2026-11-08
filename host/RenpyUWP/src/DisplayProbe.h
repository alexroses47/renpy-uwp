// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <functional>

// HDMI output probing and mode switching (Windows.Graphics.Display.Core.HdmiDisplayInformation).
// Must be called on the CoreWindow (UI) thread.
namespace renpyuwp::display {

struct Mode {
    uint32_t width = 0;
    uint32_t height = 0;
    double refreshHz = 0;
};

// Logs the current and all supported modes. Returns false if HDMI info is unavailable.
bool Init();

// Current output mode (0x0 if unavailable).
Mode Current();

// Candidate modes: 0=4K120, 1=1440p120, 2=4K60, 3=1080p120, 4=1080p60 (fallback).
int CandidateCount();
const char* CandidateLabel(int index);

// Requests candidate `index` if the display lists it. `pump` is called while waiting for the
// async request (keeps the UI thread responsive). On success fills `active` with the mode the
// system reports afterwards. The host still has to verify that frames actually reach the display.
bool RequestCandidate(int index, const std::function<void()>& pump, Mode& active);

// Re-requests the last successfully chosen mode (after resuming). Does not wait.
void Reapply();

// Asks the system to go back to the default mode (and forgets the chosen mode). Does not wait.
void RestoreDefault();

}  // namespace renpyuwp::display
