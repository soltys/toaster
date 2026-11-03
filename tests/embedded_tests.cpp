#include "pch.h"

#include "test.h"

#include "embedded.h"
#include "embedded_assets.h"

#include <algorithm>
#include <cwctype>
#include <set>

namespace {

namespace embedded = toaster::embedded;

std::wstring upper(std::wstring_view text) {
    std::wstring result(text);
    for (auto& c : result) {
        c = static_cast<wchar_t>(std::towupper(c));
    }
    return result;
}

bool validName(std::wstring_view name) {
    return !name.empty() && std::ranges::all_of(name, [](wchar_t c) {
        return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'_';
    });
}

template <class Assets>
bool hasAsset(const Assets& assets, std::wstring_view name) {
    return std::ranges::any_of(assets, [&](const auto& asset) { return asset.name == name; });
}

} // namespace

TEST(embedded, asset_names_are_valid_and_unique) {
    std::set<std::wstring> sounds;
    for (const auto& sound : embedded::kSounds) {
        CHECK(validName(sound.name));
        CHECK(sounds.insert(std::wstring(sound.name)).second);
        CHECK(sound.extension == L".wav" || sound.extension == L".mp3");
    }
    std::set<std::wstring> images;
    for (const auto& image : embedded::kImages) {
        CHECK(validName(image.name));
        CHECK(images.insert(std::wstring(image.name)).second);
        CHECK(image.extension == L".png" || image.extension == L".jpg" || image.extension == L".jpeg" ||
              image.extension == L".gif");
    }
}

TEST(embedded, find_sound_ignores_case_and_reports_the_format) {
    if (embedded::kSounds.empty()) {
        SKIP("this build embeds no sounds");
    }
    for (const auto& sound : embedded::kSounds) {
        const auto found = embedded::findSound(upper(sound.name));
        REQUIRE(found.has_value());
        CHECK_EQ(found->resourceName, L"SND_" + std::wstring(sound.name));
        CHECK(found->format == (sound.extension == L".mp3" ? embedded::SoundFormat::Mp3 : embedded::SoundFormat::Wav));
    }
}

TEST(embedded, find_sound_rejects_unknown_names) {
    CHECK(!embedded::findSound(L"no_such_sound_anywhere").has_value());
    CHECK(!embedded::findSound(L"").has_value());
    CHECK(!embedded::findSound(L"..\\evil").has_value());
}

TEST(embedded, extract_image_rejects_unknown_names) {
    CHECK(!embedded::extractImage(L"no_such_image_anywhere").has_value());
    CHECK(!embedded::extractImage(L"").has_value());
    CHECK(!embedded::extractImage(L"..\\..\\windows\\system32").has_value());
}

TEST(embedded, describe_lists_everything) {
    const std::wstring text = embedded::describe();
    CHECK_CONTAINS(text, L"Embedded sounds:");
    CHECK_CONTAINS(text, L"Embedded images:");
    for (const auto& sound : embedded::kSounds) {
        CHECK_CONTAINS(text, std::wstring(L"  ") + std::wstring(sound.name) + L"  (");
    }
    for (const auto& image : embedded::kImages) {
        CHECK_CONTAINS(text, std::wstring(L"  ") + std::wstring(image.name) + L"\n");
    }
}

// The icon names SKILL.md tells agents to use. A build that embeds the standard
// icons (scripts/Get-Images.ps1) has to have all of them.
TEST(embedded, icons_named_in_the_skill_exist) {
    if (embedded::kImages.empty()) {
        SKIP("this build embeds no images");
    }
    if (!hasAsset(embedded::kImages, L"success")) {
        SKIP("this build does not embed the standard icon set");
    }
    for (const wchar_t* icon : {L"success", L"failure", L"question", L"warning", L"info", L"stop", L"build",
                                L"deploy", L"bell", L"robot", L"waiting", L"done", L"bug", L"celebrate"}) {
        if (!hasAsset(embedded::kImages, icon)) {
            CHECK_EQ(std::wstring(L"missing icon ") + icon, L"");
        }
    }
}
