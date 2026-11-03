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
    std::wstring pack;     // sound pack name (--pack)
    std::wstring category; // category within the pack (--category)
    bool wait = false;    // block until the toast is dismissed or activated
    std::optional<std::uint32_t> timeoutSeconds; // only meaningful with --wait
    bool help = false;
    bool version = false;
    bool listEmbedded = false; // print sounds/images compiled into the .exe
    bool listPacks = false;    // print embedded sound packs
    bool installSkill = false; // install the embedded AI agent skill (wizard)
    std::wstring skillTarget;  // --skill-target: tools to install into, no questions asked
    bool printSkill = false;   // print the embedded SKILL.md
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
