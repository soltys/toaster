#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace toaster::mp3 {

// Decodes MP3 data into a complete in-memory .wav file (16-bit PCM) that
// PlaySound(SND_MEMORY) can play. Returns an empty vector when the data
// can't be decoded.
[[nodiscard]] std::vector<std::byte> decodeToWav(std::span<const std::byte> mp3);

} // namespace toaster::mp3
