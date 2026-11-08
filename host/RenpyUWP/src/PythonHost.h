// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <string>

// Embedded CPython (static python312.lib). The interpreter runs on its own thread with an
// 8 MB stack (UWP threads default to 1 MB and Ren'Py recurses deeply).
namespace renpyuwp::python {

enum class State { NotStarted, Running, Succeeded, Failed };

// Starts the interpreter thread: initializes Python, runs python\smoke.py (module "smoke",
// function main), finalizes. Paths are absolute (LocalState, package install folder).
void StartAsync(const std::wstring& localState, const std::wstring& installDir);

State GetState();

}  // namespace renpyuwp::python
