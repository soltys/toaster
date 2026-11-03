#pragma once

#include <string_view>

namespace toaster::app_id {

// AppUserModelID that toasts are sent under.
inline constexpr std::wstring_view kAppUserModelId = L"Soltys.Toaster";
inline constexpr std::wstring_view kDisplayName = L"Toaster";

// Unpackaged apps need an AppUserModelID that Windows knows about before
// their toasts are shown. Instead of an installer or a Start Menu shortcut,
// register it under HKCU\Software\Classes\AppUserModelId (no admin rights
// needed). The registry is only written when the value is missing or stale.
// Returns false if registration failed.
bool ensureRegistered();

} // namespace toaster::app_id
