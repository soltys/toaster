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

std::wstring fromUtf8(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
    return wide;
}

} // namespace

bool stdinIsConsole() {
    HANDLE handle = ::GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    return handle && handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode);
}

bool readLine(std::wstring& line) {
    line.clear();
    HANDLE handle = ::GetStdHandle(STD_INPUT_HANDLE);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool gotAnything = false;
    if (stdinIsConsole()) {
        wchar_t c = 0;
        DWORD read = 0;
        while (::ReadConsoleW(handle, &c, 1, &read, nullptr) && read == 1) {
            gotAnything = true;
            if (c == L'\n') {
                break;
            }
            if (c != L'\r') {
                line += c;
            }
        }
        return gotAnything;
    }

    std::string bytes;
    char c = 0;
    DWORD read = 0;
    while (::ReadFile(handle, &c, 1, &read, nullptr) && read == 1) {
        gotAnything = true;
        if (c == '\n') {
            break;
        }
        if (c != '\r') {
            bytes += c;
        }
    }
    line = fromUtf8(bytes);
    return gotAnything;
}

void out(std::wstring_view text) { write(STD_OUTPUT_HANDLE, text); }
void err(std::wstring_view text) { write(STD_ERROR_HANDLE, text); }

} // namespace toaster::console
