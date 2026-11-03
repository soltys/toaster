#include "pch.h"

#include "toast.h"

#include "app_id.h"
#include "handle.h"

#include <shlwapi.h>

namespace toaster::toast {
namespace {

namespace dom = winrt::Windows::Data::Xml::Dom;
namespace notifications = winrt::Windows::UI::Notifications;

void appendEscaped(std::wstring& xml, std::wstring_view text) {
    for (wchar_t c : text) {
        switch (c) {
        case L'&':  xml += L"&amp;";  break;
        case L'<':  xml += L"&lt;";   break;
        case L'>':  xml += L"&gt;";   break;
        case L'"':  xml += L"&quot;"; break;
        case L'\'': xml += L"&apos;"; break;
        default:    xml += c;         break;
        }
    }
}

std::wstring fileUri(const std::filesystem::path& path) {
    constexpr DWORD kMaxUrlLength = 2084; // INTERNET_MAX_URL_LENGTH, avoids pulling in wininet.h
    wchar_t buffer[kMaxUrlLength];
    DWORD size = kMaxUrlLength;
    if (SUCCEEDED(::UrlCreateFromPathW(path.c_str(), buffer, &size, 0))) {
        return std::wstring(buffer, size);
    }
    return path.wstring();
}

constexpr std::wstring_view kButtonArgumentPrefix = L"button:";

std::wstring buildXml(const Content& content) {
    // With buttons the toast is a question, so keep it on screen until the
    // user answers instead of letting it slide into Action Center after ~5 s.
    std::wstring xml = content.buttons.empty() ? L"<toast>" : L"<toast scenario=\"reminder\">";
    xml += L"<visual><binding template=\"ToastGeneric\">";

    if (!content.title.empty()) {
        xml += L"<text>";
        appendEscaped(xml, content.title);
        xml += L"</text>";
    }

    if (!content.body.empty()) {
        xml += L"<text>";
        appendEscaped(xml, content.body);
        xml += L"</text>";
    }

    if (content.image) {
        xml += L"<image src=\"";
        appendEscaped(xml, fileUri(*content.image));
        xml += L'"';
        switch (content.imageStyle) {
        case ImageStyle::Logo: xml += L" placement=\"appLogoOverride\""; break;
        case ImageStyle::Hero: xml += L" placement=\"hero\"";            break;
        case ImageStyle::Inline:                                         break;
        }
        xml += L"/>";
    }

    xml += L"</binding></visual>";
    if (content.silent) {
        xml += L"<audio silent=\"true\"/>";
    }
    if (!content.buttons.empty()) {
        xml += L"<actions>";
        for (std::size_t i = 0; i < content.buttons.size(); ++i) {
            xml += L"<action activationType=\"foreground\" content=\"";
            appendEscaped(xml, content.buttons[i]);
            xml += L"\" arguments=\"";
            xml += kButtonArgumentPrefix;
            xml += std::to_wstring(i + 1);
            xml += L"\"/>";
        }
        xml += L"</actions>";
    }
    xml += L"</toast>";
    return xml;
}

// Shared with the event handlers, which may run on another thread and may
// outlive the Notification object.
struct State {
    UniqueHandle signal{::CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    std::atomic<Outcome> outcome{Outcome::Pending};
    std::atomic<std::int32_t> error{0};
    std::atomic<std::size_t> button{0};

    void finish(Outcome value, std::size_t clicked = 0) {
        Outcome expected = Outcome::Pending;
        if (outcome.compare_exchange_strong(expected, value)) {
            button = clicked;
        }
        ::SetEvent(signal.get());
    }
};

// Returns the 1-based button index encoded in the activation arguments, or 0
// when the toast body itself was clicked.
std::size_t buttonFromArguments(std::wstring_view arguments) {
    if (!arguments.starts_with(kButtonArgumentPrefix)) {
        return 0;
    }
    std::size_t index = 0;
    for (wchar_t c : arguments.substr(kButtonArgumentPrefix.size())) {
        if (c < L'0' || c > L'9') {
            return 0;
        }
        index = index * 10 + static_cast<std::size_t>(c - L'0');
    }
    return index <= kMaxButtons ? index : 0;
}

} // namespace

struct Notification::Impl {
    notifications::ToastNotifier notifier{nullptr};
    notifications::ToastNotification toast{nullptr};
    std::shared_ptr<State> state;
    notifications::ToastNotification::Activated_revoker activated;
    notifications::ToastNotification::Dismissed_revoker dismissed;
    notifications::ToastNotification::Failed_revoker failed;
};

Notification::Notification(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Notification::~Notification() = default;

std::unique_ptr<Notification> Notification::show(const Content& content, bool trackEvents) {
    if (!app_id::ensureRegistered()) {
        throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED),
                                   L"could not register the toaster app ID in the registry");
    }
    auto impl = std::make_unique<Impl>();
    impl->notifier = notifications::ToastNotificationManager::CreateToastNotifier(
        winrt::hstring(app_id::kAppUserModelId));

    // Setting() throws "Element not found" for AppUserModelIDs that are only
    // registered in the registry (no Start Menu shortcut). In that case we
    // cannot tell up front and just try to show the toast.
    auto setting = notifications::NotificationSetting::Enabled;
    try {
        setting = impl->notifier.Setting();
    } catch (const winrt::hresult_error&) {
    }
    if (setting != notifications::NotificationSetting::Enabled) {
        throw winrt::hresult_error(E_ACCESSDENIED,
                                   L"notifications are turned off for Toaster or for this user "
                                   L"(check Settings > System > Notifications)");
    }

    dom::XmlDocument document;
    document.LoadXml(buildXml(content));
    impl->toast = notifications::ToastNotification(document);

    if (trackEvents) {
        auto state = std::make_shared<State>();
        if (!state->signal) {
            winrt::throw_last_error();
        }
        impl->state = state;
        impl->activated = impl->toast.Activated(winrt::auto_revoke,
            [state](auto&&, const winrt::Windows::Foundation::IInspectable& args) {
                std::size_t button = 0;
                if (const auto activation = args.try_as<notifications::ToastActivatedEventArgs>()) {
                    button = buttonFromArguments(activation.Arguments());
                }
                if (button != 0) {
                    state->finish(Outcome::ButtonClicked, button);
                } else {
                    state->finish(Outcome::Activated);
                }
            });
        impl->dismissed = impl->toast.Dismissed(winrt::auto_revoke,
            [state](auto&&, const notifications::ToastDismissedEventArgs& args) {
                switch (args.Reason()) {
                case notifications::ToastDismissalReason::UserCanceled:
                    state->finish(Outcome::Dismissed);
                    break;
                case notifications::ToastDismissalReason::TimedOut:
                    state->finish(Outcome::Expired);
                    break;
                default: // ApplicationHidden: only we hide it, on timeout
                    state->finish(Outcome::TimedOut);
                    break;
                }
            });
        impl->failed = impl->toast.Failed(winrt::auto_revoke,
            [state](auto&&, const notifications::ToastFailedEventArgs& args) {
                state->error = args.ErrorCode().value;
                state->finish(Outcome::Failed);
            });
    }

    impl->notifier.Show(impl->toast);
    return std::unique_ptr<Notification>(new Notification(std::move(impl)));
}

Outcome Notification::wait(std::optional<std::chrono::milliseconds> timeout) {
    if (!impl_->state) {
        return Outcome::Pending;
    }
    const DWORD ms = timeout ? static_cast<DWORD>(std::min<std::chrono::milliseconds::rep>(
                                   timeout->count(), INFINITE - 1))
                             : INFINITE;
    if (::WaitForSingleObject(impl_->state->signal.get(), ms) == WAIT_TIMEOUT) {
        impl_->state->finish(Outcome::TimedOut);
        try {
            impl_->notifier.Hide(impl_->toast);
        } catch (const winrt::hresult_error&) {
            // The toast may already be gone; the outcome stays TimedOut.
        }
    }
    return impl_->state->outcome.load();
}

std::int32_t Notification::errorCode() const {
    return impl_->state ? impl_->state->error.load() : 0;
}

std::size_t Notification::clickedButton() const {
    return impl_->state ? impl_->state->button.load() : 0;
}

} // namespace toaster::toast
