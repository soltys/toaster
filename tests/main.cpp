#include "pch.h"

#include "test.h"

#include <cstdio>
#include <exception>

// Usage: toaster_tests [--list] [--exclude=<prefix>]... [<prefix>]...
// Cases are named "<suite>.<name>". With prefixes only the cases whose name
// starts with one of them run; --exclude removes cases from the selection.

namespace test {

void fail(const char* file, int line, const std::string& message) {
    ++failureCount();
    std::printf("  FAILED %s(%d): %s\n", file, line, message.c_str());
    std::fflush(stdout);
}

} // namespace test

int main(int argc, char* argv[]) {
    std::vector<std::string> includes;
    std::vector<std::string> excludes;
    bool listOnly = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list") {
            listOnly = true;
        } else if (arg.starts_with("--exclude=")) {
            excludes.push_back(arg.substr(10));
        } else {
            includes.push_back(arg);
        }
    }

    const auto startsWithAny = [](const std::string& name, const std::vector<std::string>& prefixes) {
        return std::ranges::any_of(prefixes, [&](const std::string& p) { return name.starts_with(p); });
    };

    int passed = 0;
    int failed = 0;
    int skipped = 0;
    for (const auto& testCase : test::cases()) {
        if ((!includes.empty() && !startsWithAny(testCase.name, includes)) || startsWithAny(testCase.name, excludes)) {
            continue;
        }
        if (listOnly) {
            std::printf("%s\n", testCase.name.c_str());
            continue;
        }

        std::printf("[ RUN  ] %s\n", testCase.name.c_str());
        std::fflush(stdout);
        const int before = test::failureCount();
        bool wasSkipped = false;
        try {
            testCase.run();
        } catch (const test::Abort&) {
            // already recorded
        } catch (const test::Skip& skip) {
            wasSkipped = true;
            std::printf("  skipped: %s\n", skip.reason.c_str());
        } catch (const std::exception& error) {
            test::fail(__FILE__, __LINE__, std::string("unexpected exception: ") + error.what());
        } catch (...) {
            test::fail(__FILE__, __LINE__, "unexpected exception");
        }

        if (wasSkipped) {
            ++skipped;
        } else if (test::failureCount() != before) {
            ++failed;
            std::printf("[ FAIL ] %s\n", testCase.name.c_str());
        } else {
            ++passed;
        }
    }

    if (!listOnly) {
        std::printf("\n%d passed, %d failed, %d skipped\n", passed, failed, skipped);
    }
    return failed == 0 ? 0 : 1;
}
