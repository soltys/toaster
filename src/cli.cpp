#include "cli.h"

#include <array>

namespace toaster::cli {
namespace {

enum class Id { Title, Body, Image, ImageStyle, Sound, Wait, Timeout, ListEmbedded, Help, Version };

struct OptionSpec {
    Id id;
    wchar_t shortName;          // L'\0' when there is no short form
    std::wstring_view longName;
    bool takesValue;
};

constexpr std::array kSpecs{
    OptionSpec{Id::Title,   L't',  L"title",   true},
    OptionSpec{Id::Body,    L'b',  L"body",    true},
    OptionSpec{Id::Image,   L'i',  L"image",   true},
    OptionSpec{Id::ImageStyle, L'\0', L"image-style", true},
    OptionSpec{Id::Sound,   L's',  L"sound",   true},
    OptionSpec{Id::Wait,    L'w',  L"wait",    false},
    OptionSpec{Id::Timeout, L'\0', L"timeout", true},
    OptionSpec{Id::ListEmbedded, L'\0', L"list-embedded", false},
    OptionSpec{Id::Help,    L'h',  L"help",    false},
    OptionSpec{Id::Version, L'v',  L"version", false},
};

const OptionSpec* findShort(wchar_t c) {
    for (const auto& spec : kSpecs) {
        if (spec.shortName != L'\0' && spec.shortName == c) {
            return &spec;
        }
    }
    return nullptr;
}

const OptionSpec* findLong(std::wstring_view name) {
    for (const auto& spec : kSpecs) {
        if (spec.longName == name) {
            return &spec;
        }
    }
    return nullptr;
}

std::optional<std::uint32_t> parseSeconds(std::wstring_view text) {
    if (text.empty() || text.size() > 9) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<std::uint32_t>(c - L'0');
    }
    return value;
}

std::wstring displayName(const OptionSpec& spec) {
    std::wstring name = L"--";
    name += spec.longName;
    if (spec.shortName != L'\0') {
        name += L" (-";
        name += spec.shortName;
        name += L')';
    }
    return name;
}

// Applies one option to the result. Returns an error message on failure.
std::optional<std::wstring> apply(Options& options, const OptionSpec& spec, std::wstring_view value) {
    switch (spec.id) {
    case Id::Title:   options.title.assign(value); break;
    case Id::Body:    options.body.assign(value);  break;
    case Id::Image:   options.image.assign(value); break;
    case Id::ImageStyle:
        if (value != L"logo" && value != L"inline" && value != L"hero") {
            return L"invalid value for --image-style: '" + std::wstring(value) +
                   L"' (expected logo, inline or hero)";
        }
        options.imageStyle.assign(value);
        break;
    case Id::Sound:   options.sound.assign(value); break;
    case Id::Wait:    options.wait = true;         break;
    case Id::Help:    options.help = true;         break;
    case Id::Version: options.version = true;      break;
    case Id::ListEmbedded: options.listEmbedded = true; break;
    case Id::Timeout: {
        auto seconds = parseSeconds(value);
        if (!seconds) {
            return L"invalid value for --timeout: '" + std::wstring(value) +
                   L"' (expected a whole number of seconds)";
        }
        options.timeoutSeconds = *seconds;
        break;
    }
    }
    return std::nullopt;
}

} // namespace

ParseResult parse(std::span<const std::wstring_view> args) {
    Options options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::wstring_view arg = args[i];
        const OptionSpec* spec = nullptr;
        std::optional<std::wstring_view> inlineValue;

        if (arg.size() > 2 && arg.starts_with(L"--")) {
            std::wstring_view name = arg.substr(2);
            if (const auto eq = name.find(L'='); eq != std::wstring_view::npos) {
                inlineValue = name.substr(eq + 1);
                name = name.substr(0, eq);
            }
            spec = findLong(name);
        } else if (arg.size() == 2 && (arg[0] == L'-' || arg[0] == L'/')) {
            spec = findShort(arg[1]);
            if (!spec && arg[0] == L'/' && arg[1] == L'?') {
                spec = findLong(L"help");
            }
        }

        if (!spec) {
            return ParseError{L"unknown argument: '" + std::wstring(arg) + L"'"};
        }

        std::wstring_view value;
        if (spec->takesValue) {
            if (inlineValue) {
                value = *inlineValue;
            } else if (i + 1 < args.size()) {
                value = args[++i];
            } else {
                return ParseError{L"missing value for " + displayName(*spec)};
            }
        } else if (inlineValue) {
            return ParseError{displayName(*spec) + L" does not take a value"};
        }

        if (auto error = apply(options, *spec, value)) {
            return ParseError{std::move(*error)};
        }
    }

    if (options.help || options.version || options.listEmbedded) {
        return options;
    }

    if (options.title.empty()) {
        return ParseError{L"missing required --title (-t)"};
    }
    if (options.timeoutSeconds && !options.wait) {
        return ParseError{L"--timeout can only be used together with --wait"};
    }

    return options;
}

std::wstring usage() {
    return
        L"Usage: toaster -t <title> [options]\n"
        L"\n"
        L"Shows a Windows toast notification and optionally plays a .wav or .mp3 file.\n"
        L"\n"
        L"Options:\n"
        L"  -t, --title <text>        Toast title (required)\n"
        L"  -b, --body <text>         Toast body text\n"
        L"  -i, --image <path|name>   Image shown in the toast (file or embedded name)\n"
        L"      --image-style <style> logo (default), inline or hero\n"
        L"  -s, --sound <path|name>   .wav/.mp3 played when the toast is shown (file or embedded name)\n"
        L"  -w, --wait                Wait until the toast is dismissed or clicked\n"
        L"      --timeout <sec>       With --wait: give up after this many seconds\n"
        L"      --list-embedded       List sounds and images compiled into this .exe\n"
        L"  -h, --help                Show this help\n"
        L"  -v, --version             Show version\n"
        L"\n"
        L"Values can also be given as --name=value. For --sound and --image an\n"
        L"existing file wins; otherwise the value is looked up among embedded assets.\n"
        L"\n"
        L"Exit codes:\n"
        L"  0  shown (with --wait: clicked)    3  --wait: closed by the user\n"
        L"  1  bad arguments or missing file   4  --wait: expired to Action Center\n"
        L"  2  Windows failed to show it       5  --wait: --timeout elapsed\n"
        L"\n"
        L"Example:\n"
        L"  toaster -t \"Build done\" -b \"All green\" -i logo.png -s done.wav --wait\n";
}

} // namespace toaster::cli
