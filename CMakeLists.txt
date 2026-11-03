cmake_minimum_required(VERSION 4.2.3)

project(toaster
    VERSION 0.1.0
    DESCRIPTION "Windows toast notifications and .wav playback from the command line"
    LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "toaster targets Windows and must be built with MSVC.")
endif()

# Statically link the C/C++ runtime so the result is a single self-contained .exe.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EmbedAssets)

# Every .wav in TOASTER_SOUNDS_DIR and every image in TOASTER_IMAGES_DIR is
# compiled into the .exe. Relative paths are resolved against the source
# directory. Adding or removing files re-runs CMake automatically.
#   cmake --preset msvc-x64 -DTOASTER_SOUNDS_DIR=C:/my/sounds -DTOASTER_IMAGES_DIR=C:/my/images
set(TOASTER_SOUNDS_DIR "assets/sounds" CACHE PATH "Folder whose .wav files are embedded in toaster.exe")
set(TOASTER_IMAGES_DIR "assets/images" CACHE PATH "Folder whose .png/.jpg/.jpeg/.gif files are embedded in toaster.exe")

toaster_collect_assets(TOASTER_EMBED_SOUNDS "${TOASTER_SOUNDS_DIR}" wav)
toaster_collect_assets(TOASTER_EMBED_IMAGES "${TOASTER_IMAGES_DIR}" png jpg jpeg gif)

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)

add_executable(toaster
    src/main.cpp
    src/cli.cpp
    src/console.cpp
    src/embedded.cpp
    src/app_id.cpp
    src/toast.cpp
    src/sound.cpp
)

toaster_embed_assets(toaster
    SOUNDS ${TOASTER_EMBED_SOUNDS}
    IMAGES ${TOASTER_EMBED_IMAGES}
)

target_include_directories(toaster PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_features(toaster PRIVATE cxx_std_20)
set_target_properties(toaster PROPERTIES CXX_EXTENSIONS OFF)

target_precompile_headers(toaster PRIVATE src/pch.h)

target_compile_definitions(toaster PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00   # Windows 10 / 11
)

target_compile_options(toaster PRIVATE
    /W4
    /permissive-
    /utf-8
    /EHsc
    /Zc:__cplusplus
    /MP
    /bigobj
    $<$<NOT:$<CONFIG:Debug>>:/O2 /GL /Gy>
)

# Only Windows system import libraries; nothing extra ships next to the .exe.
target_link_libraries(toaster PRIVATE
    WindowsApp   # C++/WinRT / Windows Runtime
    winmm        # PlaySound
    shlwapi      # UrlCreateFromPath
)

target_link_options(toaster PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:/LTCG /OPT:REF /OPT:ICF>
)
