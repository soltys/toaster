#pragma once

#include <string>
#include <string_view>

#include "sound_packs.h"

namespace toaster::packs {

// Finds a pack by name (case-insensitive). Returns nullptr when unknown.
[[nodiscard]] const SoundPack* find(std::wstring_view name);

// Finds a category of a pack (case-insensitive). Returns nullptr when unknown.
[[nodiscard]] const PackCategory* findCategory(const SoundPack& pack, std::wstring_view name);

// Picks one sound from the category at random.
[[nodiscard]] const PackSound& pickRandom(const PackCategory& category);

// Comma-separated category names, for error messages.
[[nodiscard]] std::wstring categoryNames(const SoundPack& pack);

// Category names of one pack, one per line (for --list-categories).
[[nodiscard]] std::wstring categoryLines(const SoundPack& pack);

// Human-readable list of packs, categories and sound labels.
[[nodiscard]] std::wstring describe();

} // namespace toaster::packs
