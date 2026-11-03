#pragma once

// A tiny test framework: no dependencies, one executable, cases register
// themselves at start-up.
//
//   TEST(cli, parses_title) {
//       CHECK_EQ(1 + 1, 2);
//   }
//
// CHECK* record a failure and carry on; REQUIRE stops the case; SKIP marks the
// case as not applicable (for example, when the build embeds no sound packs).

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace test {

struct Abort {};                 // thrown by REQUIRE after recording the failure
struct Skip { std::string reason; };

struct Case {
    std::string name;
    void (*run)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> all;
    return all;
}

struct Registrar {
    Registrar(const char* name, void (*run)()) { cases().push_back({name, run}); }
};

inline int& failureCount() {
    static int count = 0;
    return count;
}

inline std::string toUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), size, nullptr,
                          nullptr);
    return utf8;
}

template <class T>
std::string show(const T& value) {
    if constexpr (std::is_convertible_v<const T&, std::wstring_view>) {
        return "\"" + toUtf8(std::wstring_view(value)) + "\"";
    } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {
        return "\"" + std::string(std::string_view(value)) + "\"";
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(value));
    } else if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(value);
    } else {
        return "<value>";
    }
}

void fail(const char* file, int line, const std::string& message);

} // namespace test

#define TEST(suite, name)                                                              \
    static void suite##_##name();                                                      \
    static const ::test::Registrar suite##_##name##_registrar(#suite "." #name, suite##_##name); \
    static void suite##_##name()

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) ::test::fail(__FILE__, __LINE__, "CHECK(" #condition ")");   \
    } while (0)

#define REQUIRE(condition)                                                             \
    do {                                                                               \
        if (!(condition)) {                                                            \
            ::test::fail(__FILE__, __LINE__, "REQUIRE(" #condition ")");               \
            throw ::test::Abort{};                                                     \
        }                                                                              \
    } while (0)

#define CHECK_EQ(actual, expected)                                                     \
    do {                                                                               \
        auto&& check_actual = (actual);                                                \
        auto&& check_expected = (expected);                                            \
        if (!(check_actual == check_expected)) {                                       \
            ::test::fail(__FILE__, __LINE__,                                           \
                         std::string("CHECK_EQ(" #actual ", " #expected ")\n    actual:   ") + \
                             ::test::show(check_actual) + "\n    expected: " + ::test::show(check_expected)); \
        }                                                                              \
    } while (0)

// `text` and `needle` are wide or narrow strings.
#define CHECK_CONTAINS(text, needle)                                                   \
    do {                                                                               \
        auto&& check_text = (text);                                                    \
        if (std::basic_string_view<std::decay_t<decltype(check_text[0])>>(check_text).find(needle) == \
            std::basic_string_view<std::decay_t<decltype(check_text[0])>>::npos) {     \
            ::test::fail(__FILE__, __LINE__,                                           \
                         std::string("CHECK_CONTAINS(" #text ", " #needle ")\n    text: ") + \
                             ::test::show(check_text) + "\n    needle: " + ::test::show(needle)); \
        }                                                                              \
    } while (0)

#define SKIP(reason) throw ::test::Skip{reason}
