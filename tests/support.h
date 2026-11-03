#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace support {

std::wstring fromUtf8(std::string_view text);

struct RunResult {
    int exitCode = -1;
    bool timedOut = false;
    std::wstring out; // stdout, "\r" removed
    std::wstring err; // stderr, "\r" removed
};

// Runs toaster.exe with the given arguments. stdin is NUL (not a console) and
// the process is killed after `timeout`, so a toast that would wait for a click
// can never hang the test run.
RunResult runToaster(const std::vector<std::wstring>& args,
                     std::chrono::milliseconds timeout = std::chrono::seconds(30));

// A fresh empty folder under %TEMP%, deleted again on destruction.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

// Sets (or, with nullopt, removes) an environment variable for the lifetime of
// the object. Child processes started meanwhile inherit it.
class ScopedEnv {
public:
    ScopedEnv(std::wstring name, std::optional<std::wstring> value);
    ~ScopedEnv();
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    std::wstring name_;
    std::optional<std::wstring> previous_;
};

void writeFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes);
void writeFile(const std::filesystem::path& path, std::string_view text);
std::string readFile(const std::filesystem::path& path);

// A silent 16-bit mono .wav of the given length.
std::vector<unsigned char> silentWav(int milliseconds);

} // namespace support
