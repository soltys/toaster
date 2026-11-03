#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace toaster::sound {

enum class Format { Wav, Mp3 };

struct FileSound {
    std::filesystem::path path;
    Format format;
};

// A .wav compiled into the executable (resource type WAVE).
struct EmbeddedWav {
    std::wstring resourceName; // e.g. L"SND_DONE"
};

using Source = std::variant<FileSound, EmbeddedWav>;

// Detects the format from the file header (falls back to the extension for
// MP3 files without an ID3 tag). Returns nullopt for anything else.
[[nodiscard]] std::optional<Format> detectFormat(const std::filesystem::path& path);

// A sound that is playing. Destroying it stops playback.
class Playback {
public:
    virtual ~Playback() = default;
};

// Starts playback. With async=false the call returns when the sound has
// finished; with async=true it returns immediately and the sound keeps
// playing while the returned object is alive.
// Returns nullptr and fills `error` when Windows cannot play the sound.
[[nodiscard]] std::unique_ptr<Playback> play(const Source& source, bool async, std::wstring& error);

} // namespace toaster::sound
