#pragma once

// Minimal file logger writing to LocalState\renpy-uwp.log.
// Must be usable from the very first line of the app, before any other subsystem.

namespace renpyuwp::log {

// Opens (truncates) LocalState\renpy-uwp.log. Safe to call more than once.
void Init();

// printf-style line logger. Adds a timestamp and newline, flushes immediately.
void Write(const char* fmt, ...);

void Shutdown();

}  // namespace renpyuwp::log

#define RLOG(...) ::renpyuwp::log::Write(__VA_ARGS__)
