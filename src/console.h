#pragma once

#include <string_view>

namespace toaster::console {

// Writes text to stdout/stderr. Uses WriteConsoleW for a real console so
// Unicode shows correctly, and UTF-8 when output is redirected to a file or pipe.
void out(std::wstring_view text);
void err(std::wstring_view text);

} // namespace toaster::console
