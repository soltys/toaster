#include "pch.h"

#include "test.h"

#include "mp3.h"
#include "sound.h"
#include "support.h"

namespace {

using toaster::sound::Format;

std::optional<Format> detect(const support::TempDir& dir, const wchar_t* fileName,
                             const std::vector<unsigned char>& bytes) {
    const auto path = dir.path() / fileName;
    support::writeFile(path, bytes);
    return toaster::sound::detectFormat(path);
}

std::vector<unsigned char> bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

// One silent MPEG-1 Layer III frame: 128 kbit/s, 44.1 kHz, mono, 417 bytes.
std::vector<unsigned char> silentMp3Frames(int count) {
    std::vector<unsigned char> mp3;
    for (int i = 0; i < count; ++i) {
        mp3.insert(mp3.end(), {0xFF, 0xFB, 0x90, 0xC4});
        mp3.insert(mp3.end(), 417 - 4, 0);
    }
    return mp3;
}

std::uint32_t u32At(const std::vector<std::byte>& data, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

std::uint16_t u16At(const std::vector<std::byte>& data, std::size_t offset) {
    std::uint16_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

} // namespace

// ---------------------------------------------------------------- detectFormat

TEST(sound, detects_wav_by_header_whatever_the_extension) {
    support::TempDir dir;
    CHECK(detect(dir, L"a.wav", support::silentWav(10)) == Format::Wav);
    CHECK(detect(dir, L"b.bin", support::silentWav(10)) == Format::Wav);
    CHECK(detect(dir, L"c.mp3", support::silentWav(10)) == Format::Wav);
}

TEST(sound, detects_mp3_by_id3_tag_or_frame_sync) {
    support::TempDir dir;
    CHECK(detect(dir, L"tagged.dat", bytes("ID3\x04\x00\x00\x00\x00\x00\x00")) == Format::Mp3);
    CHECK(detect(dir, L"frames.dat", silentMp3Frames(1)) == Format::Mp3);
    CHECK(detect(dir, L"sync2.dat", {0xFF, 0xF3, 0x00, 0x00}) == Format::Mp3);
}

TEST(sound, falls_back_to_the_extension_for_mp3) {
    support::TempDir dir;
    CHECK(detect(dir, L"headerless.mp3", bytes("not really audio")) == Format::Mp3);
    CHECK(detect(dir, L"LOUD.MP3", bytes("not really audio")) == Format::Mp3);
}

TEST(sound, rejects_everything_else) {
    support::TempDir dir;
    CHECK(!detect(dir, L"notes.txt", bytes("hello, world")).has_value());
    CHECK(!detect(dir, L"empty.wav", {}).has_value());
    CHECK(!detect(dir, L"picture.png", bytes("\x89PNG\r\n\x1a\n\0\0\0\0")).has_value());
    // A RIFF container that is not WAVE (for example AVI).
    CHECK(!detect(dir, L"video.avi", bytes("RIFF\0\0\0\0AVI LIST")).has_value());
    CHECK(!detect(dir, L"short.wav", bytes("RIFF")).has_value());
}

TEST(sound, missing_file_has_no_format) {
    support::TempDir dir;
    CHECK(!toaster::sound::detectFormat(dir.path() / L"missing.wav").has_value());
}

// ---------------------------------------------------------------- MP3 decoding

TEST(mp3, garbage_does_not_decode) {
    CHECK(toaster::mp3::decodeToWav({}).empty());
    const std::vector<std::byte> garbage(2000, std::byte{0x42});
    CHECK(toaster::mp3::decodeToWav(garbage).empty());
}

TEST(mp3, decodes_to_a_consistent_pcm_wav) {
    const auto mp3 = silentMp3Frames(6);
    const std::span<const std::byte> input(reinterpret_cast<const std::byte*>(mp3.data()), mp3.size());
    const auto wav = toaster::mp3::decodeToWav(input);
    REQUIRE(wav.size() > 44);

    CHECK_EQ(std::string(reinterpret_cast<const char*>(wav.data()), 4), "RIFF");
    CHECK_EQ(std::string(reinterpret_cast<const char*>(wav.data()) + 8, 8), "WAVEfmt ");
    CHECK_EQ(u32At(wav, 4), static_cast<std::uint32_t>(wav.size() - 8));   // RIFF size
    CHECK_EQ(u16At(wav, 20), 1);                                           // PCM
    CHECK_EQ(u16At(wav, 22), 1);                                           // mono
    CHECK_EQ(u32At(wav, 24), 44100u);                                      // sample rate
    CHECK_EQ(u16At(wav, 34), 16);                                          // bits per sample
    CHECK_EQ(u32At(wav, 28), 44100u * 2);                                  // bytes per second
    CHECK_EQ(std::string(reinterpret_cast<const char*>(wav.data()) + 36, 4), "data");
    CHECK_EQ(u32At(wav, 40), static_cast<std::uint32_t>(wav.size() - 44)); // data size
    // 1152 samples per frame; the decoder may drop the first frame while it syncs.
    CHECK(wav.size() - 44 >= 4 * 1152 * 2);
}
