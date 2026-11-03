#include "pch.h"

#include "sound.h"

#include "handle.h"

#include <mmsystem.h>

namespace toaster::sound {

bool looksLikeWav(const std::filesystem::path& path) {
    UniqueHandle file = makeFileHandle(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        return false;
    }
    char header[12] = {};
    DWORD read = 0;
    if (!::ReadFile(file.get(), header, sizeof(header), &read, nullptr) || read != sizeof(header)) {
        return false;
    }
    return std::string_view(header, 4) == "RIFF" && std::string_view(header + 8, 4) == "WAVE";
}

bool play(const Source& source, bool async) {
    const DWORD mode = SND_NODEFAULT | (async ? SND_ASYNC : SND_SYNC);
    if (const auto* file = std::get_if<FileSound>(&source)) {
        return ::PlaySoundW(file->path.c_str(), nullptr, SND_FILENAME | mode) != FALSE;
    }
    const auto& embedded = std::get<EmbeddedSound>(source);
    return ::PlaySoundW(embedded.resourceName.c_str(), ::GetModuleHandleW(nullptr), SND_RESOURCE | mode) != FALSE;
}

} // namespace toaster::sound
