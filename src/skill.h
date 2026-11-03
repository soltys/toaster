#pragma once

#include <string_view>

namespace toaster::skill {

// The "toaster" agent skill (SKILL.md) is compiled into the .exe from
// skill/toaster/SKILL.md and can be installed into the personal skill folders
// of AI developer tools:
//   Claude Code                       %USERPROFILE%\.claude\skills\toaster
//   OpenAI Codex                      %USERPROFILE%\.agents\skills\toaster
//   GitHub Copilot CLI / Copilot app,
//   VS Code (Copilot), Visual Studio  %USERPROFILE%\.copilot\skills\toaster

// --print-skill: writes the embedded SKILL.md to stdout. Returns an exit code.
[[nodiscard]] int print();

// --install-skill. With empty `targets` runs an interactive wizard that lists
// the detected tools and asks where to install. Otherwise `targets` is a
// comma-separated list of tool ids (claude, codex, copilot, vscode, vs) or
// "all" (every detected tool), and nothing is asked. Returns an exit code.
[[nodiscard]] int install(std::wstring_view targets);

} // namespace toaster::skill
