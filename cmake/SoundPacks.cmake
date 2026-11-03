# Reads sound pack descriptions (CESP JSON files, "cesp_version": "1.0") from
# a folder at configure time, verifies them and turns them into C++ tables,
# so toaster.exe never has to parse JSON at runtime.
#
#   toaster_load_sound_packs(<dir> <out_sounds> <out_images> <header_file> [<icon_dir>...])
#
# For every <dir>/*.json:
#   - each "categories.<category>.sounds[].file" is checked against its
#     "sha256" and embedded as sound "<pack>_<stem>";
#   - the optional "icon" is embedded as image "<pack>_icon";
#   - files are looked up relative to the JSON file, then by file name in the
#     JSON folder, in <folder>/<pack>/, and (icons only) in extra folders;
#     missing files are skipped with a warning;
#   - name, display_name, categories and labels go into <header_file>.
# <out_sounds> / <out_images> receive "<name>|<path>" items for
# toaster_embed_assets().

function(_toaster_cpp_string out_var value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\r" "" value "${value}")
    set(${out_var} "L\"${value}\"" PARENT_SCOPE)
endfunction()

function(_toaster_sanitize out_var value)
    string(TOLOWER "${value}" value)
    string(REGEX REPLACE "[^a-z0-9_]" "_" value "${value}")
    set(${out_var} "${value}" PARENT_SCOPE)
endfunction()

# Finds a file referenced by a pack. Tries, in order: the path relative to the
# JSON file, then just the file name in the JSON folder, in <folder>/<pack>/,
# and in any extra folders (e.g. the images folder for icons).
function(_toaster_find_pack_file out_var pack_dir pack file)
    get_filename_component(file_name "${file}" NAME)
    set(candidates
        "${pack_dir}/${file}"
        "${pack_dir}/${file_name}"
        "${pack_dir}/${pack}/${file}"
        "${pack_dir}/${pack}/${file_name}")
    foreach(extra IN LISTS ARGN)
        list(APPEND candidates "${extra}/${file_name}")
    endforeach()
    foreach(candidate IN LISTS candidates)
        get_filename_component(candidate "${candidate}" ABSOLUTE)
        if(EXISTS "${candidate}" AND NOT IS_DIRECTORY "${candidate}")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

function(toaster_load_sound_packs dir out_sounds out_images header_file)
    set(extra_icon_dirs ${ARGN}) # optional folders also searched for icons
    set(sounds "")
    set(images "")
    set(tables "")
    set(pack_entries "")
    set(seen_packs "")
    set(pack_index 0)

    set(json_files "")
    if(NOT dir STREQUAL "")
        get_filename_component(abs_dir "${dir}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        if(IS_DIRECTORY "${abs_dir}")
            file(GLOB json_files CONFIGURE_DEPENDS "${abs_dir}/*.json")
            list(SORT json_files)
        endif()
    endif()

    foreach(json_file IN LISTS json_files)
        # Re-run CMake when the pack description changes.
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${json_file}")
        get_filename_component(pack_dir "${json_file}" DIRECTORY)
        file(READ "${json_file}" json)

        string(JSON raw_name ERROR_VARIABLE err GET "${json}" name)
        if(err)
            message(FATAL_ERROR "Sound pack ${json_file}: missing \"name\" (${err})")
        endif()
        _toaster_sanitize(pack "${raw_name}")
        if(pack IN_LIST seen_packs)
            message(FATAL_ERROR "Two sound packs are named '${pack}' (${json_file})")
        endif()
        list(APPEND seen_packs "${pack}")

        string(JSON display_name ERROR_VARIABLE err GET "${json}" display_name)
        if(err)
            set(display_name "${raw_name}")
        endif()

        set(icon_asset "")
        string(JSON icon ERROR_VARIABLE err GET "${json}" icon)
        if(NOT err AND NOT icon STREQUAL "")
            _toaster_find_pack_file(icon_path "${pack_dir}" "${pack}" "${icon}" ${extra_icon_dirs})
            if(icon_path)
                set(icon_asset "${pack}_icon")
                list(APPEND images "${icon_asset}|${icon_path}")
            else()
                message(WARNING "Sound pack '${pack}': icon '${icon}' not found next to ${json_file}")
            endif()
        endif()

        string(JSON category_count ERROR_VARIABLE err LENGTH "${json}" categories)
        if(err)
            message(FATAL_ERROR "Sound pack ${json_file}: missing \"categories\" (${err})")
        endif()

        set(category_entries "")
        set(sound_total 0)
        if(category_count GREATER 0)
            math(EXPR last_category "${category_count} - 1")
            foreach(c RANGE ${last_category})
                string(JSON category MEMBER "${json}" categories ${c})
                string(JSON sound_count ERROR_VARIABLE err LENGTH "${json}" categories "${category}" sounds)
                if(err OR sound_count EQUAL 0)
                    continue()
                endif()

                set(sound_entries "")
                math(EXPR last_sound "${sound_count} - 1")
                foreach(i RANGE ${last_sound})
                    string(JSON file GET "${json}" categories "${category}" sounds ${i} file)
                    string(JSON label ERROR_VARIABLE err GET "${json}" categories "${category}" sounds ${i} label)
                    if(err)
                        set(label "")
                    endif()
                    string(JSON expected_hash ERROR_VARIABLE hash_err GET "${json}" categories "${category}" sounds ${i} sha256)

                    _toaster_find_pack_file(sound_path "${pack_dir}" "${pack}" "${file}")
                    if(NOT sound_path)
                        message(WARNING "Sound pack '${pack}': '${file}' not found next to ${json_file}; skipped")
                        continue()
                    endif()
                    if(NOT hash_err AND NOT expected_hash STREQUAL "")
                        file(SHA256 "${sound_path}" actual_hash)
                        string(TOLOWER "${expected_hash}" expected_hash)
                        if(NOT actual_hash STREQUAL expected_hash)
                            message(FATAL_ERROR "Sound pack '${pack}': sha256 mismatch for ${sound_path}\n"
                                                "  expected ${expected_hash}\n  actual   ${actual_hash}")
                        endif()
                    endif()

                    get_filename_component(stem "${sound_path}" NAME_WE)
                    _toaster_sanitize(stem "${stem}")
                    set(asset "${pack}_${stem}")
                    # A file can be listed in several categories; embed it once.
                    if(NOT "${asset}|${sound_path}" IN_LIST sounds)
                        list(APPEND sounds "${asset}|${sound_path}")
                    endif()

                    _toaster_cpp_string(asset_literal "${asset}")
                    _toaster_cpp_string(label_literal "${label}")
                    string(APPEND sound_entries "    PackSound{${asset_literal}, ${label_literal}},\n")
                    math(EXPR sound_total "${sound_total} + 1")
                endforeach()

                if(sound_entries STREQUAL "")
                    continue()
                endif()
                set(table "kPack${pack_index}Category${c}")
                string(APPEND tables "inline constexpr PackSound ${table}[] = {\n${sound_entries}};\n")
                _toaster_cpp_string(category_literal "${category}")
                string(APPEND category_entries "    PackCategory{${category_literal}, ${table}},\n")
            endforeach()
        endif()

        if(category_entries STREQUAL "")
            message(WARNING "Sound pack '${pack}' has no sounds; skipped")
            continue()
        endif()

        set(categories_table "kPack${pack_index}Categories")
        string(APPEND tables "inline constexpr PackCategory ${categories_table}[] = {\n${category_entries}};\n\n")
        _toaster_cpp_string(pack_literal "${pack}")
        _toaster_cpp_string(display_literal "${display_name}")
        _toaster_cpp_string(icon_literal "${icon_asset}")
        string(APPEND pack_entries
            "    SoundPack{${pack_literal}, ${display_literal}, ${icon_literal}, ${categories_table}},\n")
        message(STATUS "toaster: sound pack '${pack}' (${display_name}): ${sound_total} sound(s)")
        math(EXPR pack_index "${pack_index} + 1")
    endforeach()

    set(header "// Generated by cmake/SoundPacks.cmake. Do not edit.
#pragma once

#include <array>
#include <span>
#include <string_view>

namespace toaster::packs {

struct PackSound {
    std::wstring_view asset; // embedded sound name
    std::wstring_view label; // what the sound says
};

struct PackCategory {
    std::wstring_view name;  // e.g. L\"task.complete\"
    std::span<const PackSound> sounds;
};

struct SoundPack {
    std::wstring_view name;
    std::wstring_view displayName;
    std::wstring_view iconAsset; // embedded image name, empty when none
    std::span<const PackCategory> categories;
};

${tables}inline constexpr std::array<SoundPack, ${pack_index}> kPacks{
${pack_entries}};

} // namespace toaster::packs
")
    toaster_write_if_changed("${header_file}" "${header}")

    set(${out_sounds} "${sounds}" PARENT_SCOPE)
    set(${out_images} "${images}" PARENT_SCOPE)
endfunction()
