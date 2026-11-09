// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <string>

// Embedded CPython (static python312.lib + renpy_static.lib built-in modules).
// Runs on the calling thread, which must be the SDL/CoreWindow thread: SDL's WinRT video backend
// (pygame display, later Ren'Py) only works there. The executable reserves an 8 MB stack.
namespace renpyuwp::python {

// Initializes Python, runs each "module.function" in `entries` in order, finalizes.
// Returns the number of entries that failed or returned non-zero.
int RunBlocking(const std::wstring& localState, const std::wstring& installDir,
                const char* const* entries, int count);

}  // namespace renpyuwp::python
