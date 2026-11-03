#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace toaster::embedded {

enum class SoundFormat { Wav, Mp3 };

struct SoundInfo {
    std::wstring resourceName; // e.g. L"SND_DONE"
    SoundFormat format;
};

// Looks up an embedded sound by name.
[[nodiscard]] std::optional<SoundInfo> findSound(std::wstring_view name);

// MCI can only play MP3 from a file, so an embedded MP3 is unpacked to the
// cache folder (see extractImage) and that path is returned.
[[nodiscard]] std::optional<std::filesystem::path> extractSound(std::wstring_view name);

// Toasts can only show images from a file, so an embedded image is written to
// %LOCALAPPDATA%\toaster\cache once and that path is returned. The file name
// contains a hash of the content, so a rebuilt .exe never shows a stale image.
// Returns nullopt when no image with this name is embedded or writing fails.
[[nodiscard]] std::optional<std::filesystem::path> extractImage(std::wstring_view name);

// Human-readable list of embedded sounds and images.
[[nodiscard]] std::wstring describe();

} // namespace toaster::embedded
