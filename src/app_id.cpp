#include "pch.h"

#include "app_id.h"

namespace toaster::app_id {
namespace {

struct KeyCloser {
    void operator()(HKEY key) const noexcept { ::RegCloseKey(key); }
};
using UniqueKey = std::unique_ptr<std::remove_pointer_t<HKEY>, KeyCloser>;

bool hasValue(HKEY key, const wchar_t* name, std::wstring_view expected) {
    wchar_t buffer[256];
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    if (::RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size) != ERROR_SUCCESS ||
        type != REG_SZ) {
        return false;
    }
    std::wstring_view actual(buffer, size / sizeof(wchar_t));
    while (!actual.empty() && actual.back() == L'\0') {
        actual.remove_suffix(1);
    }
    return actual == expected;
}

} // namespace

bool ensureRegistered() {
    std::wstring path = L"Software\\Classes\\AppUserModelId\\";
    path += kAppUserModelId;

    HKEY raw = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                          KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &raw, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    UniqueKey key(raw);

    if (hasValue(key.get(), L"DisplayName", kDisplayName)) {
        return true;
    }
    const std::wstring value(kDisplayName);
    return ::RegSetValueExW(key.get(), L"DisplayName", 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

} // namespace toaster::app_id
