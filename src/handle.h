#pragma once

#include <windows.h>

#include <memory>

namespace toaster {

// unique_ptr-based owner for Win32 HANDLEs closed with CloseHandle.
struct HandleCloser {
    void operator()(HANDLE handle) const noexcept {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle);
        }
    }
};

using UniqueHandle = std::unique_ptr<void, HandleCloser>;

// CreateFile reports failure with INVALID_HANDLE_VALUE instead of nullptr.
[[nodiscard]] inline UniqueHandle makeFileHandle(HANDLE handle) noexcept {
    return UniqueHandle(handle == INVALID_HANDLE_VALUE ? nullptr : handle);
}

} // namespace toaster
