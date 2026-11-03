#include "pch.h"

#include "console.h"

namespace toaster::console {
namespace {

void write(DWORD stdHandle, std::wstring_view text) {
    if (text.empty()) {
        return;
    }
    HANDLE handle = ::GetStdHandle(stdHandle);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode)) {
        DWORD written = 0;
        ::WriteConsoleW(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }

    const int length = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return;
    }
    std::string utf8(static_cast<std::size_t>(length), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                          utf8.data(), length, nullptr, nullptr);
    DWORD written = 0;
    ::WriteFile(handle, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

} // namespace

void out(std::wstring_view text) { write(STD_OUTPUT_HANDLE, text); }
void err(std::wstring_view text) { write(STD_ERROR_HANDLE, text); }

} // namespace toaster::console
