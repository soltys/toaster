#include "pch.h"

#include "test.h"

#include "embedded_assets.h"
#include "packs.h"

#include <algorithm>
#include <cwctype>
#include <set>

// These tests check whatever packs this build embeds, so they work with any
// content of the sounds folder and skip when it has no packs.

namespace {

namespace packs = toaster::packs;

void requirePacks() {
    if (packs::kPacks.empty()) {
        SKIP("this build embeds no sound packs");
    }
}

std::wstring upper(std::wstring_view text) {
    std::wstring result(text);
    for (auto& c : result) {
        c = static_cast<wchar_t>(std::towupper(c));
    }
    return result;
}

template <class Assets>
bool hasAsset(const Assets& assets, std::wstring_view name) {
    return std::ranges::any_of(assets, [&](const auto& asset) { return asset.name == name; });
}

} // namespace

TEST(packs, find_ignores_case) {
    requirePacks();
    for (const auto& pack : packs::kPacks) {
        const auto* found = packs::find(upper(pack.name));
        CHECK(found == &pack);
    }
}

TEST(packs, find_rejects_unknown_names) {
    CHECK(packs::find(L"no_such_pack") == nullptr);
    CHECK(packs::find(L"") == nullptr);
    requirePacks();
    CHECK(packs::find(std::wstring(packs::kPacks[0].name) + L"x") == nullptr);
}

TEST(packs, every_pack_is_well_formed) {
    requirePacks();
    std::set<std::wstring> names;
    for (const auto& pack : packs::kPacks) {
        CHECK(!pack.name.empty());
        CHECK(!pack.displayName.empty());
        CHECK(!pack.categories.empty());
        CHECK(names.insert(std::wstring(pack.name)).second); // unique
        // Names are what users type, so they follow the same rules as asset names.
        CHECK(std::ranges::all_of(pack.name, [](wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'_'; }));
        for (const auto& category : pack.categories) {
            CHECK(!category.name.empty());
            CHECK(!category.sounds.empty());
            for (const auto& sound : category.sounds) {
                CHECK(!sound.asset.empty());
            }
        }
    }
}

TEST(packs, every_pack_sound_and_icon_is_embedded) {
    requirePacks();
    for (const auto& pack : packs::kPacks) {
        if (!pack.iconAsset.empty()) {
            CHECK(hasAsset(toaster::embedded::kImages, pack.iconAsset));
        }
        for (const auto& category : pack.categories) {
            for (const auto& sound : category.sounds) {
                if (!hasAsset(toaster::embedded::kSounds, sound.asset)) {
                    CHECK_EQ(std::wstring(sound.asset), L"<an embedded sound>");
                }
            }
        }
    }
}

TEST(packs, pack_sounds_are_prefixed_with_the_pack_name) {
    requirePacks();
    for (const auto& pack : packs::kPacks) {
        for (const auto& category : pack.categories) {
            for (const auto& sound : category.sounds) {
                CHECK(sound.asset.starts_with(std::wstring(pack.name) + L"_"));
            }
        }
    }
}

TEST(packs, find_category_ignores_case) {
    requirePacks();
    const auto& pack = packs::kPacks[0];
    for (const auto& category : pack.categories) {
        CHECK(packs::findCategory(pack, upper(category.name)) == &category);
    }
    CHECK(packs::findCategory(pack, L"no.such.category") == nullptr);
    CHECK(packs::findCategory(pack, L"") == nullptr);
}

TEST(packs, pick_random_returns_a_sound_of_that_category) {
    requirePacks();
    for (const auto& pack : packs::kPacks) {
        for (const auto& category : pack.categories) {
            for (int i = 0; i < 20; ++i) {
                const packs::PackSound& picked = packs::pickRandom(category);
                CHECK(&picked >= category.sounds.data() && &picked < category.sounds.data() + category.sounds.size());
            }
        }
    }
}

TEST(packs, pick_random_varies) {
    requirePacks();
    const packs::PackCategory* biggest = nullptr;
    for (const auto& pack : packs::kPacks) {
        for (const auto& category : pack.categories) {
            if (!biggest || category.sounds.size() > biggest->sounds.size()) {
                biggest = &category;
            }
        }
    }
    REQUIRE(biggest != nullptr);
    if (biggest->sounds.size() < 2) {
        SKIP("no category has more than one sound");
    }
    std::set<const packs::PackSound*> seen;
    for (int i = 0; i < 300; ++i) {
        seen.insert(&packs::pickRandom(*biggest));
    }
    CHECK(seen.size() > 1);
}

TEST(packs, category_names_lists_every_category) {
    requirePacks();
    for (const auto& pack : packs::kPacks) {
        const std::wstring names = packs::categoryNames(pack);
        for (const auto& category : pack.categories) {
            CHECK_CONTAINS(names, category.name);
        }
        CHECK(!names.starts_with(L","));
        CHECK(!names.ends_with(L", "));
    }
}

TEST(packs, category_lines_lists_one_category_per_line) {
    requirePacks();
    for (const auto& pack : packs::kPacks) {
        std::wstring expected;
        for (const auto& category : pack.categories) {
            expected += std::wstring(category.name) + L"\n";
        }
        CHECK_EQ(packs::categoryLines(pack), expected);
    }
}

TEST(packs, describe_lists_packs_categories_and_sounds) {
    const std::wstring text = packs::describe();
    if (packs::kPacks.empty()) {
        CHECK_CONTAINS(text, L"No sound packs");
        return;
    }
    for (const auto& pack : packs::kPacks) {
        CHECK_CONTAINS(text, std::wstring(pack.name) + L" - " + std::wstring(pack.displayName));
        for (const auto& category : pack.categories) {
            CHECK_CONTAINS(text, std::wstring(L"  ") + std::wstring(category.name) + L"\n");
            for (const auto& sound : category.sounds) {
                CHECK_CONTAINS(text, sound.asset);
            }
        }
    }
}

// SKILL.md tells agents that these packs and categories exist. If a build
// embeds one of the packs, it must have them, otherwise the skill lies.
TEST(packs, standard_packs_have_the_categories_the_skill_relies_on) {
    requirePacks();
    for (const wchar_t* name : {L"peon", L"glados", L"jan_pawel_2"}) {
        const auto* pack = packs::find(name);
        if (!pack) {
            continue;
        }
        for (const wchar_t* category : {L"task.complete", L"task.error", L"input.required"}) {
            if (!packs::findCategory(*pack, category)) {
                CHECK_EQ(std::wstring(name) + L" lacks " + category, L"");
            }
        }
    }
}
