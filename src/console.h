#pragma once

#include <string>
#include <string_view>

namespace toaster::console {

// Writes text to stdout/stderr. Uses WriteConsoleW for a real console so
// Unicode shows correctly, and UTF-8 when output is redirected to a file or pipe.
void out(std::wstring_view text);
void err(std::wstring_view text);

// True when stdin is an interactive console (not a pipe or a file).
[[nodiscard]] bool stdinIsConsole();

// Reads one line from stdin, without the line break. Returns false at end of
// input. Console input is read as UTF-16; redirected input is taken as UTF-8.
[[nodiscard]] bool readLine(std::wstring& line);

} // namespace toaster::console
