#include "pch.h"

#include "sound.h"

#include "handle.h"

#include <mmsystem.h>

namespace toaster::sound {
namespace {

// .wav files and embedded .wav resources go through PlaySound, which is the
// lightest way to play PCM audio.
class WavPlayback final : public Playback {
public:
    explicit WavPlayback(bool async) : async_(async) {}
    ~WavPlayback() override {
        if (async_) {
            ::PlaySoundW(nullptr, nullptr, 0); // stop
        }
    }

private:
    bool async_;
};

std::wstring mciError(MCIERROR code) {
    wchar_t text[256] = {};
    if (!::mciGetErrorStringW(code, text, static_cast<UINT>(std::size(text)))) {
        return L"MCI error " + std::to_wstring(code);
    }
    return text;
}

// MP3 goes through MCI (winmm), which decodes it with the codecs that ship
// with Windows, so no extra library is needed.
class Mp3Playback final : public Playback {
public:
    static constexpr const wchar_t* kAlias = L"toaster_sound";

    ~Mp3Playback() override {
        ::mciSendStringW((std::wstring(L"close ") + kAlias).c_str(), nullptr, 0, nullptr);
    }

    static std::unique_ptr<Playback> start(const std::filesystem::path& path, bool async, std::wstring& error) {
        const std::wstring open = L"open \"" + path.wstring() + L"\" type mpegvideo alias " + kAlias;
        if (const MCIERROR code = ::mciSendStringW(open.c_str(), nullptr, 0, nullptr)) {
            error = mciError(code);
            return nullptr;
        }
        auto playback = std::unique_ptr<Playback>(new Mp3Playback());

        const std::wstring play = std::wstring(L"play ") + kAlias + (async ? L"" : L" wait");
        if (const MCIERROR code = ::mciSendStringW(play.c_str(), nullptr, 0, nullptr)) {
            error = mciError(code);
            return nullptr; // destructor closes the device
        }
        return playback;
    }

private:
    Mp3Playback() = default;
};

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

    if (const auto* embedded = std::get_if<EmbeddedWav>(&source)) {
        if (!::PlaySoundW(embedded->resourceName.c_str(), ::GetModuleHandleW(nullptr), SND_RESOURCE | mode)) {
            error = L"PlaySound failed";
            return nullptr;
        }
        return std::make_unique<WavPlayback>(async);
    }

    const auto& file = std::get<FileSound>(source);
    if (file.format == Format::Mp3) {
        return Mp3Playback::start(file.path, async, error);
    }
    if (!::PlaySoundW(file.path.c_str(), nullptr, SND_FILENAME | mode)) {
        error = L"PlaySound failed";
        return nullptr;
    }
    return std::make_unique<WavPlayback>(async);
}

} // namespace toaster::sound
