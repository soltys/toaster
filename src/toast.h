#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace toaster::toast {

enum class ImageStyle {
    Logo,   // small square image next to the text (appLogoOverride)
    Inline, // full-width image below the text
    Hero,   // banner image above the text
};

struct Content {
    std::wstring title; // may be empty: the body then becomes the first line
    std::wstring body;
    std::optional<std::filesystem::path> image; // absolute path to an existing file
    ImageStyle imageStyle = ImageStyle::Logo;
    bool silent = false;                        // suppress the default Windows toast sound
    std::vector<std::wstring> buttons;          // action button labels, at most kMaxButtons
};

// Windows shows at most five action buttons on a toast.
inline constexpr std::size_t kMaxButtons = 5;

enum class Outcome {
    Pending,   // still on screen / in Action Center
    Activated, // user clicked the toast
    ButtonClicked, // user clicked one of Content::buttons, see clickedButton()
    Dismissed, // user closed the toast
    Expired,   // toast left the screen on its own (moved to Action Center)
    TimedOut,  // the --timeout given to wait() elapsed
    Failed,    // Windows reported an error while showing the toast
};

// A toast that has been handed to Windows. Destroying the object does not
// remove the toast from the screen.
class Notification {
public:
    // Shows the toast. Throws winrt::hresult_error on failure.
    // When trackEvents is false the object only keeps the toast alive and
    // wait() returns Pending immediately; this keeps fire-and-forget fast.
    static std::unique_ptr<Notification> show(const Content& content, bool trackEvents);

    ~Notification();
    Notification(const Notification&) = delete;
    Notification& operator=(const Notification&) = delete;

    // Blocks until the user reacts, the toast expires, or the timeout elapses.
    // On timeout the toast is removed from the screen.
    Outcome wait(std::optional<std::chrono::milliseconds> timeout);

    // Error code reported by Windows when wait() returned Failed.
    [[nodiscard]] std::int32_t errorCode() const;

    // 1-based index into Content::buttons when wait() returned ButtonClicked.
    [[nodiscard]] std::size_t clickedButton() const;

private:
    struct Impl;
    explicit Notification(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace toaster::toast
