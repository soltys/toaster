#include "pch.h"

#include "test.h"

#include "support.h"
#include "test_config.h"

#include <algorithm>
#include <cwctype>

// End-to-end tests: they run the real toaster.exe. Nothing here shows a toast
// or makes a sound; every case fails or answers before a notification would be
// created. Tests that do show toasts are in ui_tests.cpp.

namespace {

using support::RunResult;
using support::runToaster;

std::vector<std::wstring> lines(const std::wstring& text) {
    std::vector<std::wstring> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(L'\n', start);
        result.push_back(text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start));
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

// Names listed under "Embedded images:" by --list-embedded.
std::vector<std::wstring> embeddedImages() {
    std::vector<std::wstring> names;
    bool inImages = false;
    for (const auto& line : lines(runToaster({L"--list-embedded"}).out)) {
        if (line == L"Embedded images:") {
            inImages = true;
        } else if (inImages && line.starts_with(L"  ") && !line.starts_with(L"  (")) {
            names.push_back(line.substr(2));
        }
    }
    return names;
}

// First pack and one of its categories from --list-packs.
struct PackRef {
    std::wstring pack;
    std::wstring category;
};
std::optional<PackRef> firstPack() {
    PackRef ref;
    for (const auto& line : lines(runToaster({L"--list-packs"}).out)) {
        if (!line.empty() && line[0] != L' ' && ref.pack.empty()) {
            ref.pack = line.substr(0, line.find(L" - "));
        } else if (line.starts_with(L"  ") && !line.starts_with(L"    ") && !ref.pack.empty()) {
            ref.category = line.substr(2);
            return ref;
        }
    }
    return std::nullopt;
}

void expectUsageError(const RunResult& result, std::wstring_view message) {
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 1);
    CHECK_CONTAINS(result.err, L"toaster: ");
    CHECK_CONTAINS(result.err, message);
    CHECK(result.out.empty());
}

std::string normalized(std::string text) {
    std::erase(text, '\r');
    return text;
}

} // namespace

// ---------------------------------------------------------------- information

TEST(e2e, version) {
    const auto result = runToaster({L"--version"});
    CHECK_EQ(result.exitCode, 0);
    CHECK_EQ(result.out, std::wstring(L"toaster ") + TOASTER_TEST_VERSION + L"\n");
    CHECK(result.err.empty());
    CHECK_EQ(runToaster({L"-v"}).out, result.out);
}

TEST(e2e, help_lists_options_and_exit_codes) {
    for (const wchar_t* flag : {L"--help", L"-h", L"/?"}) {
        const auto result = runToaster({flag});
        CHECK_EQ(result.exitCode, 0);
        CHECK_CONTAINS(result.out, L"Usage: toaster");
        CHECK_CONTAINS(result.out, L"--pack");
        CHECK_CONTAINS(result.out, L"Exit codes:");
    }
}

TEST(e2e, list_embedded) {
    const auto result = runToaster({L"--list-embedded"});
    CHECK_EQ(result.exitCode, 0);
    CHECK_CONTAINS(result.out, L"Embedded sounds:");
    CHECK_CONTAINS(result.out, L"Embedded images:");
}

TEST(e2e, list_packs) {
    const auto result = runToaster({L"--list-packs"});
    CHECK_EQ(result.exitCode, 0);
    CHECK(!result.out.empty());
}

TEST(e2e, standard_icons_are_embedded) {
    const auto images = embeddedImages();
    if (images.empty()) {
        SKIP("this build embeds no images");
    }
    // The icon names that SKILL.md recommends.
    for (const wchar_t* icon : {L"success", L"failure", L"question", L"warning"}) {
        CHECK(std::ranges::find(images, icon) != images.end());
    }
}

// ---------------------------------------------------------------- bad arguments

TEST(e2e, no_arguments_is_a_usage_error) {
    const auto result = runToaster({});
    expectUsageError(result, L"nothing to do");
    CHECK_CONTAINS(result.err, L"Run 'toaster --help' for usage.");
}

TEST(e2e, usage_errors_exit_with_1) {
    expectUsageError(runToaster({L"--bogus"}), L"unknown argument: '--bogus'");
    expectUsageError(runToaster({L"-t"}), L"missing value for --title");
    expectUsageError(runToaster({L"-t", L"x", L"-B", L"OK"}), L"--button needs --wait");
    expectUsageError(runToaster({L"-t", L"x", L"--timeout", L"5"}), L"--timeout can only be used together with --wait");
    expectUsageError(runToaster({L"-t", L"x", L"-w", L"--timeout", L"soon"}), L"invalid value for --timeout");
    expectUsageError(runToaster({L"-t", L"x", L"--image-style", L"banner"}), L"invalid value for --image-style");
    expectUsageError(runToaster({L"-p", L"peon"}), L"--pack needs --category");
}

// ---------------------------------------------------------------- missing assets

TEST(e2e, missing_image_is_reported) {
    expectUsageError(runToaster({L"-t", L"x", L"-i", L"no_such_image_anywhere"}),
                     L"image not found: 'no_such_image_anywhere'");
}

TEST(e2e, missing_sound_is_reported) {
    expectUsageError(runToaster({L"-t", L"x", L"-s", L"no_such_sound_anywhere"}),
                     L"sound not found: 'no_such_sound_anywhere'");
    // Same in sound-only mode.
    expectUsageError(runToaster({L"-s", L"no_such_sound_anywhere"}), L"sound not found");
}

TEST(e2e, a_file_that_is_not_audio_is_rejected) {
    support::TempDir dir;
    const auto notAudio = dir.path() / L"notes.txt";
    support::writeFile(notAudio, "hello, world");
    expectUsageError(runToaster({L"-t", L"x", L"-s", notAudio.wstring()}), L"not a .wav or .mp3 file");
}

TEST(e2e, error_messages_keep_unicode_intact) {
    expectUsageError(runToaster({L"-t", L"x", L"-i", L"zażółć"}), L"'zażółć'");
}

TEST(e2e, unknown_pack_is_reported) {
    expectUsageError(runToaster({L"-p", L"no_such_pack", L"-c", L"task.complete"}),
                     L"unknown sound pack: 'no_such_pack'");
}

TEST(e2e, unknown_category_lists_the_available_ones) {
    const auto ref = firstPack();
    if (!ref) {
        SKIP("this build embeds no sound packs");
    }
    const auto result = runToaster({L"-p", ref->pack, L"-c", L"no.such.category"});
    expectUsageError(result, L"has no category 'no.such.category'");
    CHECK_CONTAINS(result.err, L"available: ");
    CHECK_CONTAINS(result.err, ref->category);
}

TEST(e2e, list_categories_of_a_pack) {
    const auto ref = firstPack();
    if (!ref) {
        SKIP("this build embeds no sound packs");
    }
    const auto result = runToaster({L"--pack", ref->pack, L"--list-categories"});
    CHECK_EQ(result.exitCode, 0);
    CHECK(result.err.empty());
    CHECK_CONTAINS(result.out, ref->category + L"\n");
}

TEST(e2e, list_categories_of_an_unknown_pack_is_reported) {
    expectUsageError(runToaster({L"-p", L"no_such_pack", L"--list-categories"}),
                     L"unknown sound pack: 'no_such_pack'");
}

TEST(e2e, list_categories_needs_a_pack) {
    expectUsageError(runToaster({L"--list-categories"}), L"--list-categories needs --pack");
}

TEST(e2e, pack_name_is_case_insensitive) {
    const auto ref = firstPack();
    if (!ref) {
        SKIP("this build embeds no sound packs");
    }
    std::wstring upper = ref->pack;
    std::ranges::transform(upper, upper.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    // The pack and category are found; the run then stops at the missing image.
    const auto result = runToaster({L"-p", upper, L"-c", ref->category, L"-i", L"no_such_image_anywhere"});
    expectUsageError(result, L"image not found");
}

// ---------------------------------------------------------------- embedded images

TEST(e2e, embedded_image_is_unpacked_to_the_cache_once) {
    const auto images = embeddedImages();
    if (images.empty()) {
        SKIP("this build embeds no images");
    }
    support::TempDir localAppData;
    const support::ScopedEnv env(L"LOCALAPPDATA", localAppData.path().wstring());

    // The image is resolved first, then the missing sound stops the run before any toast.
    const std::vector<std::wstring> args{L"-t", L"x", L"-i", images.front(), L"-s", L"no_such_sound_anywhere"};
    const auto first = runToaster(args);
    expectUsageError(first, L"sound not found"); // not "image not found": the image resolved

    const auto cache = localAppData.path() / L"toaster" / L"cache";
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(cache)) {
        files.push_back(entry.path());
    }
    REQUIRE(files.size() == 1);
    CHECK(files[0].filename().wstring().starts_with(images.front() + L"-"));
    CHECK(std::filesystem::file_size(files[0]) > 0);
    const auto written = std::filesystem::last_write_time(files[0]);

    expectUsageError(runToaster(args), L"sound not found");
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(cache)) {
        ++count;
    }
    CHECK_EQ(count, 1u);                                       // no second copy
    CHECK(std::filesystem::last_write_time(files[0]) == written); // and not rewritten
}

TEST(e2e, image_names_are_case_insensitive) {
    const auto images = embeddedImages();
    if (images.empty()) {
        SKIP("this build embeds no images");
    }
    std::wstring upper = images.front();
    std::ranges::transform(upper, upper.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    support::TempDir localAppData;
    const support::ScopedEnv env(L"LOCALAPPDATA", localAppData.path().wstring());
    expectUsageError(runToaster({L"-t", L"x", L"-i", upper, L"-s", L"no_such_sound_anywhere"}), L"sound not found");
}

TEST(e2e, an_existing_file_wins_over_an_embedded_name) {
    const auto images = embeddedImages();
    if (images.empty()) {
        SKIP("this build embeds no images");
    }
    // A file called like an embedded image is used as is, so nothing is unpacked.
    support::TempDir work;
    support::TempDir localAppData;
    const support::ScopedEnv env(L"LOCALAPPDATA", localAppData.path().wstring());
    const auto file = work.path() / images.front();
    support::writeFile(file, "not really a png");
    expectUsageError(runToaster({L"-t", L"x", L"-i", file.wstring(), L"-s", L"no_such_sound_anywhere"}),
                     L"sound not found");
    CHECK(!std::filesystem::exists(localAppData.path() / L"toaster"));
}

// ---------------------------------------------------------------- the embedded skill

TEST(e2e, print_skill_matches_skill_md) {
    const auto result = runToaster({L"--print-skill"});
    CHECK_EQ(result.exitCode, 0);
    const std::string expected = normalized(support::readFile(TOASTER_TEST_SKILL_FILE));
    REQUIRE(!expected.empty());
    CHECK(support::fromUtf8(expected) == result.out);
    CHECK(result.out.starts_with(L"---\nname: toaster\n"));
}

TEST(e2e, skill_documents_what_this_build_has) {
    // The skill is written for the standard build; if this build has the
    // standard packs, every pack and icon named in the skill must exist.
    const auto skill = support::fromUtf8(normalized(support::readFile(TOASTER_TEST_SKILL_FILE)));
    const auto packs = runToaster({L"--list-packs"}).out;
    for (const wchar_t* pack : {L"peon", L"glados", L"jan_pawel_2"}) {
        if (packs.find(std::wstring(pack) + L" - ") != std::wstring::npos) {
            CHECK_CONTAINS(skill, std::wstring(L"`") + pack + L"`");
        }
    }
}

TEST(e2e, install_skill_writes_updates_and_recognises_current_files) {
    support::TempDir home;
    const support::ScopedEnv profile(L"USERPROFILE", home.path().wstring());
    const auto target = home.path() / L".claude" / L"skills" / L"toaster" / L"SKILL.md";
    const std::string expected = support::readFile(TOASTER_TEST_SKILL_FILE);

    const auto installed = runToaster({L"--install-skill", L"--skill-target", L"claude"});
    CHECK_EQ(installed.exitCode, 0);
    CHECK_CONTAINS(installed.out, L"installed");
    CHECK(support::readFile(target) == expected);

    const auto again = runToaster({L"--install-skill", L"--skill-target", L"claude"});
    CHECK_EQ(again.exitCode, 0);
    CHECK_CONTAINS(again.out, L"up to date");

    support::writeFile(target, "an old version of the skill");
    const auto updated = runToaster({L"--install-skill", L"--skill-target", L"claude"});
    CHECK_EQ(updated.exitCode, 0);
    CHECK_CONTAINS(updated.out, L"updated");
    CHECK(support::readFile(target) == expected);
}

TEST(e2e, install_skill_for_tools_that_share_a_folder_writes_it_once) {
    support::TempDir home;
    const support::ScopedEnv profile(L"USERPROFILE", home.path().wstring());
    const auto result = runToaster({L"--install-skill", L"--skill-target", L"copilot,vscode,vs"});
    CHECK_EQ(result.exitCode, 0);
    CHECK(std::filesystem::exists(home.path() / L".copilot" / L"skills" / L"toaster" / L"SKILL.md"));
    CHECK(!std::filesystem::exists(home.path() / L".claude"));
    CHECK_CONTAINS(result.out, L"GitHub Copilot");
    CHECK_CONTAINS(result.out, L"Visual Studio Code");
    std::size_t paths = 0;
    for (std::size_t at = 0; (at = result.out.find(L"SKILL.md", at)) != std::wstring::npos; ++at) {
        ++paths;
    }
    CHECK_EQ(paths, 1u);
}

TEST(e2e, install_skill_for_codex_uses_the_agents_folder) {
    support::TempDir home;
    const support::ScopedEnv profile(L"USERPROFILE", home.path().wstring());
    CHECK_EQ(runToaster({L"--install-skill", L"--skill-target", L"codex"}).exitCode, 0);
    CHECK(std::filesystem::exists(home.path() / L".agents" / L"skills" / L"toaster" / L"SKILL.md"));
}

TEST(e2e, install_skill_all_uses_the_tools_it_detects) {
    support::TempDir home;
    std::filesystem::create_directories(home.path() / L".claude");
    const support::ScopedEnv profile(L"USERPROFILE", home.path().wstring());
    const auto result = runToaster({L"--install-skill", L"--skill-target", L"all"});
    CHECK_EQ(result.exitCode, 0);
    CHECK(std::filesystem::exists(home.path() / L".claude" / L"skills" / L"toaster" / L"SKILL.md"));
}

TEST(e2e, install_skill_rejects_an_unknown_target) {
    support::TempDir home;
    const support::ScopedEnv profile(L"USERPROFILE", home.path().wstring());
    expectUsageError(runToaster({L"--install-skill", L"--skill-target", L"emacs"}), L"unknown skill target 'emacs'");
    CHECK(!std::filesystem::exists(home.path() / L".claude"));
}

TEST(e2e, install_skill_without_a_target_needs_a_console) {
    support::TempDir home;
    const support::ScopedEnv profile(L"USERPROFILE", home.path().wstring());
    // stdin is NUL here, so the wizard must refuse instead of waiting for answers.
    const auto result = runToaster({L"--install-skill"}, std::chrono::seconds(10));
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 1);
    CHECK_CONTAINS(result.err, L"needs a console");
    CHECK(!std::filesystem::exists(home.path() / L".claude"));
}

TEST(e2e, skill_target_needs_install_skill) {
    expectUsageError(runToaster({L"--skill-target", L"all"}), L"only be used together with --install-skill");
}
