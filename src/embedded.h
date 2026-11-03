#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace toaster::embedded {

// True when a sound with this name was compiled into the executable.
[[nodiscard]] bool hasSound(std::wstring_view name);

// Resource name to pass to PlaySound(..., SND_RESOURCE), e.g. L"SND_DONE".
[[nodiscard]] std::wstring soundResourceName(std::wstring_view name);

// Toasts can only show images from a file, so an embedded image is written to
// %LOCALAPPDATA%\toaster\cache once and that path is returned. The file name
// contains a hash of the content, so a rebuilt .exe never shows a stale image.
// Returns nullopt when no image with this name is embedded or writing fails.
[[nodiscard]] std::optional<std::filesystem::path> extractImage(std::wstring_view name);

// Human-readable list of embedded sounds and images.
[[nodiscard]] std::wstring describe();

} // namespace toaster::embedded
