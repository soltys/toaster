#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace toaster::cli {

struct Options {
    std::wstring title;
    std::wstring body;
    std::wstring image;   // image file path, or the name of an embedded image
    std::wstring imageStyle = L"logo"; // logo | inline | hero
    std::wstring sound;   // .wav file path, or the name of an embedded sound
    bool wait = false;    // block until the toast is dismissed or activated
    std::optional<std::uint32_t> timeoutSeconds; // only meaningful with --wait
    bool help = false;
    bool version = false;
    bool listEmbedded = false; // print sounds/images compiled into the .exe
};

struct ParseError {
    std::wstring message;
};

using ParseResult = std::variant<Options, ParseError>;

// Parses arguments (excluding argv[0]).
// Supports "-t value", "--title value" and "--title=value".
[[nodiscard]] ParseResult parse(std::span<const std::wstring_view> args);

[[nodiscard]] std::wstring usage();

} // namespace toaster::cli
