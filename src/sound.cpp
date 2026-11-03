#include "pch.h"

#include "sound.h"

#include "handle.h"

#include <mmsystem.h>
#include <objbase.h>

#include <future>
#include <stop_token>
#include <thread>

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
//
// MCI's mpegvideo driver is built on DirectShow and fails to load in a
// multithreaded COM apartment, which is what the toast code uses. So every
// MCI call runs on a dedicated single-threaded-apartment thread.
class Mp3Playback final : public Playback {
public:
    ~Mp3Playback() override {
        worker_.request_stop(); // std::jthread joins on destruction
    }

    static std::unique_ptr<Playback> start(const std::filesystem::path& path, bool async, std::wstring& error) {
        auto playback = std::unique_ptr<Mp3Playback>(new Mp3Playback());

        std::promise<std::wstring> started;
        auto startResult = started.get_future();
        playback->worker_ = std::jthread(
            [path, started = std::move(started)](std::stop_token stop) mutable { run(path, started, stop); });

        error = startResult.get();
        if (!error.empty()) {
            return nullptr;
        }
        if (!async) {
            playback->worker_.join(); // returns when the sound has finished
        }
        return playback;
    }

private:
    static constexpr const wchar_t* kAlias = L"toaster_sound";

    Mp3Playback() = default;

    static MCIERROR send(const std::wstring& command, wchar_t* result = nullptr, UINT resultSize = 0) {
        return ::mciSendStringW(command.c_str(), result, resultSize, nullptr);
    }

    static void run(const std::filesystem::path& path, std::promise<std::wstring>& started, std::stop_token stop) {
        const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        const std::wstring alias(kAlias);
        if (const MCIERROR code = send(L"open \"" + path.wstring() + L"\" type mpegvideo alias " + alias)) {
            started.set_value(mciError(code));
        } else {
            if (const MCIERROR playCode = send(L"play " + alias)) {
                started.set_value(mciError(playCode));
            } else {
                started.set_value(L"");
                wchar_t mode[32] = {};
                while (!stop.stop_requested()) {
                    if (send(L"status " + alias + L" mode", mode, static_cast<UINT>(std::size(mode))) != 0) {
                        break;
                    }
                    const std::wstring_view state(mode);
                    if (state != L"playing" && state != L"seeking" && state != L"not ready") {
                        break; // "stopped" once the end is reached
                    }
                    ::Sleep(20);
                }
            }
            send(L"close " + alias);
        }

        if (SUCCEEDED(com)) {
            ::CoUninitialize();
        }
    }

    std::jthread worker_;
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
