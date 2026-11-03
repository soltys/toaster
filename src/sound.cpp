#include "pch.h"

#include "sound.h"

#include "handle.h"
#include "mp3.h"

#include <mmsystem.h>

namespace toaster::sound {
namespace {

// Everything plays through PlaySound, the lightest way to play PCM audio.
// MP3 is first decoded (dr_mp3) into an in-memory .wav, which is kept alive
// here because PlaySound(SND_MEMORY | SND_ASYNC) reads it while playing.
class WavPlayback final : public Playback {
public:
    explicit WavPlayback(bool async, std::vector<std::byte> buffer = {})
        : async_(async), buffer_(std::move(buffer)) {}
    ~WavPlayback() override {
        if (async_) {
            ::PlaySoundW(nullptr, nullptr, 0); // stop before buffer_ is freed
        }
    }

    [[nodiscard]] LPCWSTR memory() const { return reinterpret_cast<LPCWSTR>(buffer_.data()); }

private:
    bool async_;
    std::vector<std::byte> buffer_;
};

std::optional<std::vector<std::byte>> readFile(const std::filesystem::path& path) {
    UniqueHandle file = makeFileHandle(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    LARGE_INTEGER size{};
    if (!file || !::GetFileSizeEx(file.get(), &size) || size.QuadPart > 0x7FFFFFFF) {
        return std::nullopt;
    }
    std::vector<std::byte> data(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    if (!::ReadFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &read, nullptr) ||
        read != data.size()) {
        return std::nullopt;
    }
    return data;
}

std::span<const std::byte> loadResource(const std::wstring& name, LPCWSTR type) {
    HMODULE module = ::GetModuleHandleW(nullptr);
    HRSRC info = ::FindResourceW(module, name.c_str(), type);
    HGLOBAL loaded = info ? ::LoadResource(module, info) : nullptr; // owned by the module
    const void* data = loaded ? ::LockResource(loaded) : nullptr;
    if (!data) {
        return {};
    }
    return {static_cast<const std::byte*>(data), ::SizeofResource(module, info)};
}

std::unique_ptr<Playback> playMp3(std::span<const std::byte> mp3, DWORD mode, std::wstring& error) {
    auto wav = mp3::decodeToWav(mp3);
    if (wav.empty()) {
        error = L"could not decode the MP3 data";
        return nullptr;
    }
    auto playback = std::make_unique<WavPlayback>((mode & SND_ASYNC) != 0, std::move(wav));
    if (!::PlaySoundW(playback->memory(), nullptr, SND_MEMORY | mode)) {
        error = L"PlaySound failed";
        return nullptr;
    }
    return playback;
}

bool hasExtension(const std::filesystem::path& path, std::wstring_view ext) {
    const std::wstring actual = path.extension().wstring();
    return actual.size() == ext.size() &&
           ::CompareStringOrdinal(actual.c_str(), static_cast<int>(actual.size()), ext.data(),
                                  static_cast<int>(ext.size()), TRUE) == CSTR_EQUAL;
}

} // namespace

std::optional<Format> detectFormat(const std::filesystem::path& path) {
    UniqueHandle file = makeFileHandle(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        return std::nullopt;
    }
    unsigned char header[12] = {};
    DWORD read = 0;
    if (!::ReadFile(file.get(), header, sizeof(header), &read, nullptr)) {
        return std::nullopt;
    }

    const auto bytes = [&](std::size_t offset, std::string_view expected) {
        return read >= offset + expected.size() &&
               std::memcmp(header + offset, expected.data(), expected.size()) == 0;
    };
    if (bytes(0, "RIFF") && bytes(8, "WAVE")) {
        return Format::Wav;
    }
    // ID3v2 tag, or an MPEG audio frame sync (11 set bits).
    if (bytes(0, "ID3") || (read >= 2 && header[0] == 0xFF && (header[1] & 0xE0) == 0xE0)) {
        return Format::Mp3;
    }
    if (hasExtension(path, L".mp3")) {
        return Format::Mp3;
    }
    return std::nullopt;
}

std::unique_ptr<Playback> play(const Source& source, bool async, std::wstring& error) {
    const DWORD mode = SND_NODEFAULT | (async ? SND_ASYNC : SND_SYNC);

    if (const auto* embedded = std::get_if<EmbeddedSound>(&source)) {
        if (embedded->format == Format::Mp3) {
            const auto data = loadResource(embedded->resourceName, RT_RCDATA);
            if (data.empty()) {
                error = L"embedded sound not found";
                return nullptr;
            }
            return playMp3(data, mode, error);
        }
        if (!::PlaySoundW(embedded->resourceName.c_str(), ::GetModuleHandleW(nullptr), SND_RESOURCE | mode)) {
            error = L"PlaySound failed";
            return nullptr;
        }
        return std::make_unique<WavPlayback>(async);
    }

    const auto& file = std::get<FileSound>(source);
    if (file.format == Format::Mp3) {
        const auto data = readFile(file.path);
        if (!data) {
            error = L"could not read the file";
            return nullptr;
        }
        return playMp3(*data, mode, error);
    }
    if (!::PlaySoundW(file.path.c_str(), nullptr, SND_FILENAME | mode)) {
        error = L"PlaySound failed";
        return nullptr;
    }
    return std::make_unique<WavPlayback>(async);
}

} // namespace toaster::sound
