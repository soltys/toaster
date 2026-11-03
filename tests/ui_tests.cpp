#include "pch.h"

#include "test.h"

#include "support.h"

// Tests that really show a toast and play (silent) audio through Windows. They
// are only registered with -DTOASTER_TEST_UI=ON because they need a desktop
// session with notifications enabled, and the toasts are visible for a moment.
// Nothing here waits for a click: --wait is always combined with a 1 second
// --timeout, so the toast is removed again by toaster itself.

namespace {

using support::runToaster;

std::vector<std::wstring> withSilentSound(support::TempDir& dir, std::vector<std::wstring> args) {
    const auto wav = dir.path() / L"silence.wav";
    support::writeFile(wav, support::silentWav(100));
    args.insert(args.end(), {L"-s", wav.wstring()});
    return args;
}

} // namespace

TEST(ui, toast_is_shown) {
    const auto result = runToaster({L"-t", L"toaster test", L"-b", L"A test toast, safe to ignore"});
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 0);
    CHECK(result.err.empty());
}

TEST(ui, toast_with_body_only) {
    CHECK_EQ(runToaster({L"-b", L"toaster test: body only"}).exitCode, 0);
}

TEST(ui, wait_times_out_with_exit_code_5) {
    const auto result = runToaster({L"-t", L"toaster test", L"-b", L"Waiting 1 second", L"--wait", L"--timeout", L"1"});
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 5);
}

TEST(ui, buttons_time_out_too) {
    const auto result = runToaster({L"-t", L"toaster test", L"-B", L"Yes", L"-B", L"No", L"--wait", L"--timeout", L"1"});
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 5);
    CHECK(result.out.empty()); // no label is printed when no button was clicked
}

TEST(ui, toast_with_an_embedded_icon) {
    const auto result = runToaster({L"-t", L"toaster test", L"-b", L"With the success icon", L"-i", L"success"});
    if (result.exitCode == 1) {
        SKIP("this build has no 'success' icon");
    }
    CHECK_EQ(result.exitCode, 0);
}

TEST(ui, every_image_style) {
    for (const wchar_t* style : {L"logo", L"inline", L"hero"}) {
        CHECK_EQ(runToaster({L"-t", L"toaster test", L"-b", style, L"--image-style", style}).exitCode, 0);
    }
}

TEST(ui, sound_only_plays_a_wav_file_and_returns) {
    support::TempDir dir;
    const auto result = runToaster(withSilentSound(dir, {}));
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 0);
}

TEST(ui, toast_with_a_wav_file) {
    support::TempDir dir;
    const auto result = runToaster(withSilentSound(dir, {L"-t", L"toaster test", L"-b", L"With a silent wav"}));
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 0);
}

TEST(ui, toast_with_a_pack) {
    const auto list = runToaster({L"--list-packs"}).out;
    const auto nl = list.find(L'\n');
    if (list.empty() || list[0] == L'N' /* "No sound packs..." */ || nl == std::wstring::npos) {
        SKIP("this build embeds no sound packs");
    }
    // Look the first pack and category up, then play it with a 1 second wait.
    const std::wstring pack = list.substr(0, list.find(L" - "));
    const auto categoryStart = list.find(L"  ", nl);
    const auto categoryEnd = list.find(L'\n', categoryStart);
    const std::wstring category = list.substr(categoryStart + 2, categoryEnd - categoryStart - 2);
    const auto result = runToaster({L"-p", pack, L"-c", category, L"-w", L"--timeout", L"1"});
    CHECK(!result.timedOut);
    CHECK_EQ(result.exitCode, 5);
}
