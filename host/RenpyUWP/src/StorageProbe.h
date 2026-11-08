// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

// Probes which drives the sandboxed app can see and how (Win32 vs *FromApp vs WinRT),
// lists drive roots and reads <drive>:\renpy-uwp-test.txt. Results go to the log.
namespace renpyuwp::storage {

// Runs the probe on a background thread. Ignored if a probe is already running.
void RunAsync();

}  // namespace renpyuwp::storage
