#include "pch.h"

#include "cli.h"
#include "console.h"
#include "embedded.h"
#include "exit_code.h"
#include "version.h"

namespace {

using toaster::ExitCode;

int toInt(ExitCode code) { return static_cast<int>(code); }

int run(const toaster::cli::Options& options) {
    if (options.help) {
        toaster::console::out(toaster::cli::usage());
        return toInt(ExitCode::Ok);
    }
    if (options.version) {
        toaster::console::out(std::wstring(L"toaster ") + TOASTER_VERSION + L"\n");
        return toInt(ExitCode::Ok);
    }
    if (options.listEmbedded) {
        toaster::console::out(toaster::embedded::describe());
        return toInt(ExitCode::Ok);
    }

    // Showing the toast and playing the sound arrive in the next steps.
    toaster::console::err(L"toaster: showing notifications is not implemented yet\n");
    return toInt(ExitCode::Failure);
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::wstring_view> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const auto parsed = toaster::cli::parse(args);
    if (const auto* error = std::get_if<toaster::cli::ParseError>(&parsed)) {
        toaster::console::err(L"toaster: " + error->message + L"\n" +
                              L"Run 'toaster --help' for usage.\n");
        return toInt(ExitCode::UsageError);
    }

    return run(std::get<toaster::cli::Options>(parsed));
}
