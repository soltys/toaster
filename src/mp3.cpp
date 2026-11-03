#include "pch.h"

#include "mp3.h"

// dr_mp3 (public domain / MIT-0) is compiled into this translation unit only.
// It is third-party code, so its warnings are silenced here.
#pragma warning(push, 0)
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO // we read files ourselves (wide paths) and decode from memory
#include "dr_mp3.h"
#pragma warning(pop)

namespace toaster::mp3 {
namespace {

struct FreeSamples {
    void operator()(drmp3_int16* samples) const noexcept { drmp3_free(samples, nullptr); }
};

using Samples = std::unique_ptr<drmp3_int16, FreeSamples>;

class Writer {
public:
    explicit Writer(std::byte* out) : out_(out) {}

    void bytes(const void* data, std::size_t size) {
        std::memcpy(out_, data, size);
        out_ += size;
    }
    void tag(const char (&fourcc)[5]) { bytes(fourcc, 4); }
    // WAV is little-endian, as is every Windows target.
    void u16(std::uint16_t value) { bytes(&value, sizeof(value)); }
    void u32(std::uint32_t value) { bytes(&value, sizeof(value)); }

private:
    std::byte* out_;
};

} // namespace

std::vector<std::byte> decodeToWav(std::span<const std::byte> mp3) {
    drmp3_config config{};
    drmp3_uint64 frames = 0;
    const Samples samples(drmp3_open_memory_and_read_pcm_frames_s16(mp3.data(), mp3.size(), &config, &frames, nullptr));
    if (!samples || frames == 0 || config.channels == 0 || config.channels > 2 || config.sampleRate == 0) {
        return {};
    }

    constexpr std::uint32_t kHeaderSize = 44;
    const std::uint64_t dataSize = frames * config.channels * sizeof(drmp3_int16);
    if (dataSize > 0xFFFFFFFFull - kHeaderSize) {
        return {}; // too long for a RIFF file
    }

    const auto channels = static_cast<std::uint16_t>(config.channels);
    const auto blockAlign = static_cast<std::uint16_t>(channels * sizeof(drmp3_int16));
    const auto dataBytes = static_cast<std::uint32_t>(dataSize);

    std::vector<std::byte> wav(kHeaderSize + dataBytes);
    Writer out(wav.data());
    out.tag("RIFF");
    out.u32(kHeaderSize - 8 + dataBytes);
    out.tag("WAVE");
    out.tag("fmt ");
    out.u32(16);         // fmt chunk size
    out.u16(1);          // WAVE_FORMAT_PCM
    out.u16(channels);
    out.u32(config.sampleRate);
    out.u32(config.sampleRate * blockAlign); // bytes per second
    out.u16(blockAlign);
    out.u16(16);         // bits per sample
    out.tag("data");
    out.u32(dataBytes);
    out.bytes(samples.get(), dataBytes);
    return wav;
}

} // namespace toaster::mp3
