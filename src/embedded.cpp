#include "pch.h"

#include "embedded.h"

#include "embedded_assets.h"
#include "handle.h"

#include <algorithm>
#include <cwctype>

namespace toaster::embedded {
namespace {

std::wstring normalize(std::wstring_view name) {
    std::wstring result(name);
    std::ranges::transform(result, result.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return result;
}

const EmbeddedImage* findImage(std::wstring_view name) {
    const std::wstring wanted = normalize(name);
    const auto it = std::ranges::find(kImages, std::wstring_view(wanted), &EmbeddedImage::name);
    return it == kImages.end() ? nullptr : &*it;
}

std::span<const std::byte> loadResource(const std::wstring& name, LPCWSTR type) {
    HMODULE module = ::GetModuleHandleW(nullptr);
    HRSRC info = ::FindResourceW(module, name.c_str(), type);
    if (!info) {
        return {};
    }
    HGLOBAL loaded = ::LoadResource(module, info); // owned by the module, never freed
    const void* data = loaded ? ::LockResource(loaded) : nullptr;
    if (!data) {
        return {};
    }
    return {static_cast<const std::byte*>(data), ::SizeofResource(module, info)};
}

std::uint64_t fnv1a(std::span<const std::byte> data) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::byte b : data) {
        hash ^= static_cast<std::uint64_t>(b);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::optional<std::filesystem::path> cacheDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    std::filesystem::path base;
    if (length > 0 && length < MAX_PATH) {
        base = buffer;
    } else {
        std::error_code ec;
        base = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return std::nullopt;
        }
    }
    auto dir = base / L"toaster" / L"cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return std::nullopt;
    }
    return dir;
}

bool writeFileAtomically(const std::filesystem::path& target, std::span<const std::byte> data) {
    // Write to a unique temporary name, then rename, so concurrent toaster
    // processes never see a half-written image.
    auto temp = target;
    temp += L"." + std::to_wstring(::GetCurrentProcessId()) + L".tmp";

    {
        UniqueHandle file = makeFileHandle(::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
                                                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) {
            return false;
        }
        DWORD written = 0;
        if (!::WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) ||
            written != data.size()) {
            file.reset();
            ::DeleteFileW(temp.c_str());
            return false;
        }
    }

    if (!::MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        ::DeleteFileW(temp.c_str());
        // Another process may have won the race and created the same file.
        return std::filesystem::exists(target);
    }
    return true;
}

} // namespace

bool hasSound(std::wstring_view name) {
    const std::wstring wanted = normalize(name);
    return std::ranges::find(kSounds, std::wstring_view(wanted)) != kSounds.end();
}

std::wstring soundResourceName(std::wstring_view name) {
    return L"SND_" + normalize(name);
}

std::optional<std::filesystem::path> extractImage(std::wstring_view name) {
    const EmbeddedImage* image = findImage(name);
    if (!image) {
        return std::nullopt;
    }

    const auto data = loadResource(L"IMG_" + std::wstring(image->name), RT_RCDATA);
    if (data.empty()) {
        return std::nullopt;
    }

    const auto dir = cacheDirectory();
    if (!dir) {
        return std::nullopt;
    }

    wchar_t hash[17];
    swprintf_s(hash, L"%016llx", static_cast<unsigned long long>(fnv1a(data)));
    auto target = *dir / (std::wstring(image->name) + L"-" + hash + std::wstring(image->extension));

    std::error_code ec;
    if (std::filesystem::file_size(target, ec) == data.size() && !ec) {
        return target;
    }
    if (!writeFileAtomically(target, data)) {
        return std::nullopt;
    }
    return target;
}

std::wstring describe() {
    std::wstring text = L"Embedded sounds:\n";
    if (kSounds.empty()) {
        text += L"  (none)\n";
    }
    for (auto name : kSounds) {
        text += L"  ";
        text += name;
        text += L'\n';
    }
    text += L"Embedded images:\n";
    if (kImages.empty()) {
        text += L"  (none)\n";
    }
    for (const auto& image : kImages) {
        text += L"  ";
        text += image.name;
        text += L'\n';
    }
    return text;
}

} // namespace toaster::embedded
