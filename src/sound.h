#pragma once

#include <filesystem>
#include <string>
#include <variant>

namespace toaster::sound {

struct FileSound {
    std::filesystem::path path;
};

struct EmbeddedSound {
    std::wstring resourceName; // e.g. L"SND_DONE"
};

using Source = std::variant<FileSound, EmbeddedSound>;

// Returns true if the file starts with a RIFF/WAVE header.
[[nodiscard]] bool looksLikeWav(const std::filesystem::path& path);

// Starts playback. With async=false the call returns when the sound has
// finished; with async=true it returns immediately and the sound stops when
// the process exits. Returns false if Windows could not play the sound.
bool play(const Source& source, bool async);

} // namespace toaster::sound
