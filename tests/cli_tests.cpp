#include "pch.h"

#include "test.h"

#include "cli.h"
#include "exit_code.h"

#include <initializer_list>

namespace {

using namespace toaster::cli;

ParseResult parseArgs(std::initializer_list<std::wstring_view> args) {
    const std::vector<std::wstring_view> list(args);
    return parse(list);
}

Options options(const ParseResult& result) {
    const auto* parsed = std::get_if<Options>(&result);
    if (!parsed) {
        test::fail(__FILE__, __LINE__, "expected the arguments to parse, got: " +
                                           test::toUtf8(std::get<ParseError>(result).message));
        throw test::Abort{};
    }
    return *parsed;
}

// The arguments must be rejected with a message that contains `fragment`.
void expectError(std::initializer_list<std::wstring_view> args, std::wstring_view fragment) {
    const auto result = parseArgs(args);
    const auto* error = std::get_if<ParseError>(&result);
    CHECK(error != nullptr);
    if (error) {
        CHECK_CONTAINS(error->message, fragment);
    }
}

} // namespace

// ---------------------------------------------------------------- basics

TEST(cli, title_and_body_in_every_spelling) {
    const auto shortForm = parseArgs({L"-t", L"Hi", L"-b", L"There"});
    CHECK_EQ(options(shortForm).title, L"Hi");
    CHECK_EQ(options(shortForm).body, L"There");

    const auto longForm = parseArgs({L"--title", L"Hi", L"--body", L"There"});
    CHECK_EQ(options(longForm).title, L"Hi");
    CHECK_EQ(options(longForm).body, L"There");

    const auto equalsForm = parseArgs({L"--title=Hi", L"--body=There words"});
    CHECK_EQ(options(equalsForm).title, L"Hi");
    CHECK_EQ(options(equalsForm).body, L"There words");
}

TEST(cli, defaults) {
    const auto result = parseArgs({L"-t", L"x"});
    const Options o = options(result);
    CHECK_EQ(o.imageStyle, L"logo");
    CHECK(!o.wait);
    CHECK(!o.timeoutSeconds.has_value());
    CHECK(o.buttons.empty());
    CHECK(o.image.empty());
    CHECK(o.sound.empty());
    CHECK(o.pack.empty());
}

TEST(cli, last_value_wins) {
    CHECK_EQ(options(parseArgs({L"-t", L"a", L"-t", L"b"})).title, L"b");
}

TEST(cli, value_may_start_with_a_dash_or_contain_equals) {
    CHECK_EQ(options(parseArgs({L"-t", L"-5 errors"})).title, L"-5 errors");
    CHECK_EQ(options(parseArgs({L"--body=a=b"})).body, L"a=b");
}

TEST(cli, empty_inline_value_is_allowed_but_empty_toast_is_not) {
    CHECK_EQ(options(parseArgs({L"--title=", L"-b", L"x"})).title, L"");
    expectError({L"--title="}, L"nothing to do");
}

TEST(cli, unicode_survives) {
    CHECK_EQ(options(parseArgs({L"-t", L"Za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144"})).title,
             L"Za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144");
}

// ---------------------------------------------------------------- unknown input

TEST(cli, unknown_argument) {
    expectError({L"--nope"}, L"unknown argument: '--nope'");
    expectError({L"-z"}, L"unknown argument: '-z'");
    expectError({L"stray"}, L"unknown argument: 'stray'");
    expectError({L"-t", L"x", L"extra"}, L"unknown argument: 'extra'");
}

TEST(cli, bare_dashes_are_unknown) {
    expectError({L"-"}, L"unknown argument");
    expectError({L"--"}, L"unknown argument");
}

TEST(cli, missing_value) {
    expectError({L"-t"}, L"missing value for --title (-t)");
    expectError({L"-b", L"x", L"--image"}, L"missing value for --image (-i)");
    expectError({L"--timeout"}, L"missing value for --timeout");
}

TEST(cli, flag_does_not_take_a_value) {
    expectError({L"--wait=yes", L"-t", L"x"}, L"does not take a value");
}

// ---------------------------------------------------------------- information options

TEST(cli, help_and_version_in_every_spelling) {
    CHECK(options(parseArgs({L"-h"})).help);
    CHECK(options(parseArgs({L"--help"})).help);
    CHECK(options(parseArgs({L"/?"})).help);
    CHECK(options(parseArgs({L"-v"})).version);
    CHECK(options(parseArgs({L"--version"})).version);
}

TEST(cli, information_options_need_no_toast) {
    CHECK(options(parseArgs({L"--list-embedded"})).listEmbedded);
    CHECK(options(parseArgs({L"--list-packs"})).listPacks);
    CHECK(options(parseArgs({L"--list-categories", L"--pack", L"peon"})).listCategories);
    CHECK(options(parseArgs({L"--print-skill"})).printSkill);
    CHECK(options(parseArgs({L"--install-skill"})).installSkill);
}

TEST(cli, information_options_skip_the_other_checks) {
    // --button without --wait is an error for a toast, but not next to --help.
    CHECK(options(parseArgs({L"--help", L"--button", L"x"})).help);
}

TEST(cli, list_categories_needs_a_pack) {
    expectError({L"--list-categories"}, L"--list-categories needs --pack");
    const auto result = parseArgs({L"-p", L"peon", L"--list-categories"});
    CHECK(options(result).listCategories);
    CHECK_EQ(options(result).pack, L"peon");
}

TEST(cli, skill_target) {
    const auto result = parseArgs({L"--install-skill", L"--skill-target", L"claude,copilot"});
    CHECK(options(result).installSkill);
    CHECK_EQ(options(result).skillTarget, L"claude,copilot");

    expectError({L"--skill-target", L"all"}, L"only be used together with --install-skill");
    expectError({L"--install-skill", L"--skill-target="}, L"--skill-target needs a value");
}

// ---------------------------------------------------------------- image and style

TEST(cli, image_style) {
    for (const wchar_t* style : {L"logo", L"inline", L"hero"}) {
        CHECK_EQ(options(parseArgs({L"-t", L"x", L"--image-style", style})).imageStyle, style);
    }
    expectError({L"-t", L"x", L"--image-style", L"banner"}, L"invalid value for --image-style: 'banner'");
    expectError({L"-t", L"x", L"--image-style", L"HERO"}, L"invalid value for --image-style");
}

TEST(cli, image_and_sound_are_kept_verbatim) {
    const auto result = parseArgs({L"-t", L"x", L"-i", L"C:\\pics\\a b.png", L"-s", L"done"});
    CHECK_EQ(options(result).image, L"C:\\pics\\a b.png");
    CHECK_EQ(options(result).sound, L"done");
}

// ---------------------------------------------------------------- toast vs sound-only

TEST(cli, nothing_to_do) {
    expectError({}, L"nothing to do");
    expectError({L"-i", L"success"}, L"nothing to do");
}

TEST(cli, sound_only_mode) {
    const auto result = parseArgs({L"-s", L"peon_peonyes1"});
    CHECK_EQ(options(result).sound, L"peon_peonyes1");
    CHECK(options(result).title.empty());
    CHECK(options(result).body.empty());
}

TEST(cli, sound_only_mode_rejects_options_that_need_a_toast) {
    expectError({L"-s", L"x", L"-i", L"success"}, L"--image needs a toast");
    expectError({L"-s", L"x", L"-B", L"OK"}, L"--button needs a toast");
    expectError({L"-s", L"x", L"--wait"}, L"--wait needs a toast");
}

TEST(cli, body_alone_is_a_toast) {
    CHECK_EQ(options(parseArgs({L"-b", L"just a body"})).body, L"just a body");
}

// ---------------------------------------------------------------- wait, timeout, buttons

TEST(cli, wait_and_timeout) {
    const auto result = parseArgs({L"-t", L"x", L"-w", L"--timeout", L"120"});
    CHECK(options(result).wait);
    CHECK_EQ(options(result).timeoutSeconds.value_or(0u), 120u);
    CHECK_EQ(options(parseArgs({L"-t", L"x", L"--wait", L"--timeout=0"})).timeoutSeconds.value_or(99u), 0u);
}

TEST(cli, timeout_must_be_a_whole_number_of_seconds) {
    for (const wchar_t* bad : {L"", L"abc", L"-1", L"1.5", L"10s", L" 5", L"1234567890"}) {
        const auto result = parseArgs({L"-t", L"x", L"--wait", L"--timeout", bad});
        CHECK(std::holds_alternative<ParseError>(result));
    }
    expectError({L"-t", L"x", L"--wait", L"--timeout", L"soon"}, L"invalid value for --timeout: 'soon'");
    CHECK_EQ(options(parseArgs({L"-t", L"x", L"--wait", L"--timeout", L"999999999"})).timeoutSeconds.value_or(0u),
             999999999u);
}

TEST(cli, timeout_needs_wait) {
    expectError({L"-t", L"x", L"--timeout", L"5"}, L"--timeout can only be used together with --wait");
}

TEST(cli, buttons) {
    const auto result = parseArgs({L"-t", L"x", L"-B", L"Yes", L"--button", L"No", L"--button=Later", L"-w"});
    const Options o = options(result);
    REQUIRE(o.buttons.size() == 3);
    CHECK_EQ(o.buttons[0], L"Yes");
    CHECK_EQ(o.buttons[1], L"No");
    CHECK_EQ(o.buttons[2], L"Later");
}

TEST(cli, at_most_five_buttons) {
    CHECK_EQ(options(parseArgs({L"-t", L"x", L"-w", L"-B", L"1", L"-B", L"2", L"-B", L"3", L"-B", L"4", L"-B", L"5"}))
                 .buttons.size(),
             5u);
    expectError({L"-t", L"x", L"-w", L"-B", L"1", L"-B", L"2", L"-B", L"3", L"-B", L"4", L"-B", L"5", L"-B", L"6"},
                L"too many buttons");
}

TEST(cli, button_needs_a_label_and_wait) {
    expectError({L"-t", L"x", L"-w", L"--button="}, L"--button needs a label");
    expectError({L"-t", L"x", L"-B", L"OK"}, L"--button needs --wait");
}

// ---------------------------------------------------------------- packs

TEST(cli, pack_and_category) {
    const auto result = parseArgs({L"--pack", L"peon", L"-c", L"task.complete"});
    CHECK_EQ(options(result).pack, L"peon");
    CHECK_EQ(options(result).category, L"task.complete");
    CHECK_EQ(options(parseArgs({L"-p", L"peon", L"--category=task.error"})).category, L"task.error");
}

TEST(cli, pack_alone_makes_a_toast_so_title_is_optional) {
    // The pack supplies title, body and image later, so this is not "nothing to do".
    CHECK(std::holds_alternative<Options>(parseArgs({L"-p", L"peon", L"-c", L"task.complete"})));
}

TEST(cli, pack_and_category_go_together) {
    expectError({L"-p", L"peon"}, L"--pack needs --category");
    expectError({L"-t", L"x", L"-c", L"task.error"}, L"--category can only be used together with --pack");
}

TEST(cli, pack_and_sound_exclude_each_other) {
    expectError({L"-p", L"peon", L"-c", L"task.error", L"-s", L"done"}, L"either --sound or --pack");
}

TEST(cli, pack_values_can_be_overridden) {
    const auto result = parseArgs({L"-p", L"peon", L"-c", L"task.error", L"-t", L"T", L"-b", L"B", L"-i", L"failure"});
    CHECK_EQ(options(result).title, L"T");
    CHECK_EQ(options(result).body, L"B");
    CHECK_EQ(options(result).image, L"failure");
}

// ---------------------------------------------------------------- usage text and contract

TEST(cli, usage_documents_every_option) {
    const std::wstring text = usage();
    for (const wchar_t* option : {L"--title", L"--body", L"--image", L"--image-style", L"--sound", L"--pack",
                                  L"--category", L"--button", L"--wait", L"--timeout", L"--list-embedded",
                                  L"--list-packs", L"--list-categories", L"--install-skill", L"--skill-target", L"--print-skill",
                                  L"--help", L"--version"}) {
        CHECK_CONTAINS(text, option);
    }
    CHECK_CONTAINS(text, L"Exit codes:");
}

// Scripts depend on these numbers (see SKILL.md and --help); renumbering them is a breaking change.
TEST(cli, exit_codes_are_stable) {
    using toaster::ExitCode;
    CHECK_EQ(static_cast<int>(ExitCode::Ok), 0);
    CHECK_EQ(static_cast<int>(ExitCode::UsageError), 1);
    CHECK_EQ(static_cast<int>(ExitCode::Failure), 2);
    CHECK_EQ(static_cast<int>(ExitCode::Dismissed), 3);
    CHECK_EQ(static_cast<int>(ExitCode::Expired), 4);
    CHECK_EQ(static_cast<int>(ExitCode::TimedOut), 5);
    CHECK_EQ(static_cast<int>(ExitCode::FirstButton), 11);
}
