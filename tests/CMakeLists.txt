# Test suite: one executable, run through CTest.
#
#   ctest --preset release            # unit + end-to-end tests (no toasts shown)
#   cmake --preset msvc-x64 -DTOASTER_TEST_UI=ON   # also register the tests that show real toasts
#
# The unit tests compile the parts of toaster that don't need a toast (argument
# parser, packs, embedded asset tables, sound format detection, MP3 decoder)
# straight from src/. The end-to-end tests run the built toaster.exe.

set(TOASTER_SRC "${CMAKE_SOURCE_DIR}/src")

add_executable(toaster_tests
    main.cpp
    support.cpp
    cli_tests.cpp
    packs_tests.cpp
    embedded_tests.cpp
    sound_tests.cpp
    e2e_tests.cpp
    ui_tests.cpp
    ${TOASTER_SRC}/cli.cpp
    ${TOASTER_SRC}/packs.cpp
    ${TOASTER_SRC}/embedded.cpp
    ${TOASTER_SRC}/sound.cpp
    ${TOASTER_SRC}/mp3.cpp
)
add_dependencies(toaster_tests toaster) # the end-to-end tests run toaster.exe

# Paths and the version the end-to-end tests compare against.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/config/$<CONFIG>/test_config.h" CONTENT
"#pragma once
#define TOASTER_TEST_EXE L\"$<TARGET_FILE:toaster>\"
#define TOASTER_TEST_SKILL_FILE L\"${CMAKE_SOURCE_DIR}/skill/toaster/SKILL.md\"
#define TOASTER_TEST_VERSION L\"${PROJECT_VERSION}\"
")

target_include_directories(toaster_tests PRIVATE
    ${TOASTER_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}/generated            # embedded_assets.h, sound_packs.h
    ${CMAKE_CURRENT_BINARY_DIR}/config/$<CONFIG>
    ${CMAKE_SOURCE_DIR}/third_party/dr_libs
)

target_compile_features(toaster_tests PRIVATE cxx_std_20)
set_target_properties(toaster_tests PROPERTIES CXX_EXTENSIONS OFF)
target_precompile_headers(toaster_tests PRIVATE ${TOASTER_SRC}/pch.h)

target_compile_definitions(toaster_tests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)
target_compile_options(toaster_tests PRIVATE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus /MP /bigobj)
target_link_libraries(toaster_tests PRIVATE WindowsApp winmm shlwapi)

add_test(NAME unit COMMAND toaster_tests --exclude=e2e. --exclude=ui.)
add_test(NAME e2e COMMAND toaster_tests e2e.)
set_tests_properties(unit e2e PROPERTIES TIMEOUT 300)

if(TOASTER_TEST_UI)
    add_test(NAME ui COMMAND toaster_tests ui.)
    set_tests_properties(ui PROPERTIES TIMEOUT 120)
endif()
