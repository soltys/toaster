#pragma once

namespace toaster {

// Process exit codes. Scripts depend on these values, so never renumber them.
enum class ExitCode : int {
    Ok = 0,          // toast shown; with --wait: the user clicked it
    UsageError = 1,  // bad command line, or the image/sound could not be found
    Failure = 2,     // Windows refused or failed to show the toast
    Dismissed = 3,   // --wait: the user closed the toast
    Expired = 4,     // --wait: the toast left the screen on its own
    TimedOut = 5,    // --wait --timeout: time ran out, the toast was removed
    FirstButton = 11, // --button: button N was clicked -> FirstButton + N - 1 (11..15)
};

} // namespace toaster
