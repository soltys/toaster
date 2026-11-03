#pragma once

namespace toaster {

// Process exit codes. Scripts depend on these values, so never renumber them.
enum class ExitCode : int {
    Ok = 0,
    UsageError = 1,  // bad command line
    Failure = 2,     // runtime error (asset missing, Windows API failure, ...)
};

} // namespace toaster
