#include "pch.h"

#include "support.h"

#include "test_config.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <thread>

namespace support {
namespace {

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : handle_(h == INVALID_HANDLE_VALUE ? nullptr : h) {}
    ~Handle() { reset(); }
    Handle(Handle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    void reset() {
        if (handle_) {
            ::CloseHandle(handle_);
            handle_ = nullptr;
        }
    }
    [[nodiscard]] HANDLE get() const { return handle_; }
    [[nodiscard]] explicit operator bool() const { return handle_ != nullptr; }

private:
    HANDLE handle_ = nullptr;
};

// Quotes one argument following the rules of CommandLineToArgvW.
std::wstring quote(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring result = L"\"";
    for (auto it = arg.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            result.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        } else {
            result.append(backslashes, L'\\');
            result.push_back(*it);
        }
    }
    result.push_back(L'"');
    return result;
}

std::string drain(HANDLE pipe) {
    std::string data;
    char buffer[4096];
    DWORD read = 0;
    while (::ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        data.append(buffer, read);
    }
    return data;
}

std::wstring withoutCarriageReturns(std::wstring text) {
    std::erase(text, L'\r');
    return text;
}

} // namespace

std::wstring fromUtf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

RunResult runToaster(const std::vector<std::wstring>& args, std::chrono::milliseconds timeout) {
    RunResult result;

    SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
    Handle outRead, outWrite, errRead, errWrite;
    HANDLE h1 = nullptr, h2 = nullptr;
    if (!::CreatePipe(&h1, &h2, &inheritable, 0)) {
        return result;
    }
    outRead = Handle(h1);
    outWrite = Handle(h2);
    if (!::CreatePipe(&h1, &h2, &inheritable, 0)) {
        return result;
    }
    errRead = Handle(h1);
    errWrite = Handle(h2);
    ::SetHandleInformation(outRead.get(), HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(errRead.get(), HANDLE_FLAG_INHERIT, 0);

    Handle nul(::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr));

    std::wstring commandLine = quote(TOASTER_TEST_EXE);
    for (const auto& arg : args) {
        commandLine += L' ';
        commandLine += quote(arg);
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nul.get();
    startup.hStdOutput = outWrite.get();
    startup.hStdError = errWrite.get();

    PROCESS_INFORMATION info{};
    if (!::CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                          &startup, &info)) {
        return result;
    }
    Handle process(info.hProcess);
    Handle thread(info.hThread);
    outWrite.reset(); // so the readers see end-of-file when the child exits
    errWrite.reset();

    std::string out, err;
    std::thread outReader([&] { out = drain(outRead.get()); });
    std::thread errReader([&] { err = drain(errRead.get()); });

    if (::WaitForSingleObject(process.get(), static_cast<DWORD>(timeout.count())) == WAIT_TIMEOUT) {
        result.timedOut = true;
        ::TerminateProcess(process.get(), 0xDEAD);
        ::WaitForSingleObject(process.get(), INFINITE);
    }
    outReader.join();
    errReader.join();

    DWORD exitCode = 0;
    ::GetExitCodeProcess(process.get(), &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.out = withoutCarriageReturns(fromUtf8(out));
    result.err = withoutCarriageReturns(fromUtf8(err));
    return result;
}

TempDir::TempDir() {
    static std::atomic<unsigned> counter{0};
    const auto base = std::filesystem::temp_directory_path();
    path_ = base / (L"toaster-tests-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
                    std::to_wstring(counter++));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
}

TempDir::~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
}

ScopedEnv::ScopedEnv(std::wstring name, std::optional<std::wstring> value) : name_(std::move(name)) {
    wchar_t buffer[2048];
    const DWORD length = ::GetEnvironmentVariableW(name_.c_str(), buffer, 2048);
    if (length > 0 && length < 2048) {
        previous_ = std::wstring(buffer, length);
    }
    ::SetEnvironmentVariableW(name_.c_str(), value ? value->c_str() : nullptr);
}

ScopedEnv::~ScopedEnv() {
    ::SetEnvironmentVariableW(name_.c_str(), previous_ ? previous_->c_str() : nullptr);
}

void writeFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void writeFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<unsigned char> silentWav(int milliseconds) {
    constexpr std::uint32_t kRate = 8000;
    const std::uint32_t samples = kRate * static_cast<std::uint32_t>(milliseconds) / 1000;
    const std::uint32_t dataBytes = samples * 2;

    std::vector<unsigned char> wav;
    const auto put = [&](const void* data, std::size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        wav.insert(wav.end(), bytes, bytes + size);
    };
    const auto u16 = [&](std::uint16_t v) { put(&v, 2); };
    const auto u32 = [&](std::uint32_t v) { put(&v, 4); };
    put("RIFF", 4);
    u32(36 + dataBytes);
    put("WAVEfmt ", 8);
    u32(16);
    u16(1); // PCM
    u16(1); // mono
    u32(kRate);
    u32(kRate * 2);
    u16(2);
    u16(16);
    put("data", 4);
    u32(dataBytes);
    wav.insert(wav.end(), dataBytes, 0);
    return wav;
}

} // namespace support
