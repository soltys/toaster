#include "pch.h"

#include "packs.h"

#include <random>

namespace toaster::packs {
namespace {

bool equalsIgnoreCase(std::wstring_view a, std::wstring_view b) {
    return a.size() == b.size() &&
           ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()),
                                  TRUE) == CSTR_EQUAL;
}

} // namespace

const SoundPack* find(std::wstring_view name) {
    for (const auto& pack : kPacks) {
        if (equalsIgnoreCase(pack.name, name)) {
            return &pack;
        }
    }
    return nullptr;
}

const PackCategory* findCategory(const SoundPack& pack, std::wstring_view name) {
    for (const auto& category : pack.categories) {
        if (equalsIgnoreCase(category.name, name)) {
            return &category;
        }
    }
    return nullptr;
}

const PackSound& pickRandom(const PackCategory& category) {
    if (category.sounds.size() == 1) {
        return category.sounds.front();
    }
    std::random_device device;
    std::uniform_int_distribution<std::size_t> pick(0, category.sounds.size() - 1);
    return category.sounds[pick(device)];
}

std::wstring categoryNames(const SoundPack& pack) {
    std::wstring names;
    for (const auto& category : pack.categories) {
        if (!names.empty()) {
            names += L", ";
        }
        names += category.name;
    }
    return names;
}

std::wstring categoryLines(const SoundPack& pack) {
    std::wstring lines;
    for (const auto& category : pack.categories) {
        lines += category.name;
        lines += L'\n';
    }
    return lines;
}

std::wstring describe() {
    if (kPacks.empty()) {
        return L"No sound packs are embedded.\n";
    }
    std::wstring text;
    for (const auto& pack : kPacks) {
        text += pack.name;
        text += L" - ";
        text += pack.displayName;
        text += L'\n';
        for (const auto& category : pack.categories) {
            text += L"  ";
            text += category.name;
            text += L'\n';
            for (const auto& sound : category.sounds) {
                text += L"    ";
                text += sound.asset;
                if (!sound.label.empty()) {
                    text += L"  \"";
                    text += sound.label;
                    text += L'"';
                }
                text += L'\n';
            }
        }
    }
    return text;
}

} // namespace toaster::packs
