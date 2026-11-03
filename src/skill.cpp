#include "pch.h"

#include "skill.h"

#include "console.h"
#include "exit_code.h"

#include <array>
#include <cwctype>
#include <fstream>
#include <initializer_list>
#include <iterator>

namespace toaster::skill {
namespace {

namespace fs = std::filesystem;

int toInt(ExitCode code) { return static_cast<int>(code); }

// Personal skill folders, under %USERPROFILE%.
enum class Folder { Claude, Agents, Copilot };

std::wstring_view folderName(Folder folder) {
    switch (folder) {
    case Folder::Claude:  return L".claude";
    case Folder::Agents:  return L".agents";
    case Folder::Copilot: return L".copilot";
    }
    return L".agents";
}

struct Tool {
    std::wstring_view id;   // used by --skill-target
    std::wstring_view name; // shown to the user
    Folder folder;          // where the tool reads personal skills
};

constexpr std::array kTools{
    Tool{L"claude",  L"Claude Code",                      Folder::Claude},
    Tool{L"codex",   L"OpenAI Codex",                     Folder::Agents},
    Tool{L"copilot", L"GitHub Copilot CLI / Copilot app", Folder::Copilot},
    Tool{L"vscode",  L"Visual Studio Code (Copilot)",     Folder::Copilot},
    Tool{L"vs",      L"Visual Studio (Copilot)",          Folder::Copilot},
};

constexpr std::wstring_view kSkillName = L"toaster";

// ---------------------------------------------------------------- helpers

std::optional<fs::path> envPath(const wchar_t* name) {
    wchar_t buffer[MAX_PATH];
    const DWORD length = ::GetEnvironmentVariableW(name, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::nullopt;
    }
    return fs::path(buffer);
}

std::optional<fs::path> homeDirectory() {
    if (auto home = envPath(L"USERPROFILE")) {
        return home;
    }
    auto drive = envPath(L"HOMEDRIVE");
    auto path = envPath(L"HOMEPATH");
    if (drive && path) {
        return fs::path(drive->wstring() + path->wstring());
    }
    return std::nullopt;
}

bool pathExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// Finds an executable on PATH (and the usual Windows search locations).
std::optional<fs::path> findOnPath(const wchar_t* fileName) {
    wchar_t buffer[MAX_PATH];
    const DWORD length = ::SearchPathW(nullptr, fileName, nullptr, MAX_PATH, buffer, nullptr);
    if (length == 0 || length >= MAX_PATH) {
        return std::nullopt;
    }
    return fs::path(buffer);
}

// Returns the first candidate that exists.
std::optional<fs::path> firstExisting(std::initializer_list<std::optional<fs::path>> candidates) {
    for (const auto& candidate : candidates) {
        if (candidate && pathExists(*candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<fs::path> under(const std::optional<fs::path>& base, const wchar_t* relative) {
    if (!base) {
        return std::nullopt;
    }
    return *base / relative;
}

// Evidence that a tool is installed: a config folder or an executable.
std::optional<fs::path> detect(const Tool& tool, const fs::path& home) {
    const auto appData = envPath(L"APPDATA");
    const auto localAppData = envPath(L"LOCALAPPDATA");

    if (tool.id == L"claude") {
        return firstExisting({home / L".claude", findOnPath(L"claude.exe"), findOnPath(L"claude.cmd"),
                              under(localAppData, L"AnthropicClaude")});
    }
    if (tool.id == L"codex") {
        return firstExisting({envPath(L"CODEX_HOME"), home / L".codex", findOnPath(L"codex.exe"),
                              findOnPath(L"codex.cmd")});
    }
    if (tool.id == L"copilot") {
        return firstExisting({findOnPath(L"copilot.exe"), findOnPath(L"copilot.cmd"), home / L".copilot"});
    }
    if (tool.id == L"vscode") {
        return firstExisting({under(appData, L"Code\\User"), under(appData, L"Code - Insiders\\User"),
                              under(localAppData, L"Programs\\Microsoft VS Code"),
                              findOnPath(L"code.cmd")});
    }
    if (tool.id == L"vs") {
        return firstExisting({under(envPath(L"ProgramFiles(x86)"), L"Microsoft Visual Studio\\Installer\\vswhere.exe"),
                              under(localAppData, L"Microsoft\\VisualStudio")});
    }
    return std::nullopt;
}

fs::path skillDirectory(Folder folder, const fs::path& home) {
    return home / folderName(folder) / L"skills" / kSkillName;
}

std::span<const std::byte> embeddedSkill() {
    HMODULE module = ::GetModuleHandleW(nullptr);
    HRSRC info = ::FindResourceW(module, L"SKILL_TOASTER", RT_RCDATA);
    if (!info) {
        return {};
    }
    HGLOBAL loaded = ::LoadResource(module, info); // owned by the module, never freed
    const void* data = loaded ? ::LockResource(loaded) : nullptr;
    if (!data) {
        return {};
    }
    return {static_cast<const std::byte*>(data), ::SizeofResource(module, info)};
}

std::wstring fromUtf8(std::span<const std::byte> bytes) {
    if (bytes.empty()) {
        return {};
    }
    const auto* data = reinterpret_cast<const char*>(bytes.data());
    const int size = static_cast<int>(bytes.size());
    const int length = ::MultiByteToWideChar(CP_UTF8, 0, data, size, nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, data, size, wide.data(), length);
    return wide;
}

std::wstring lower(std::wstring_view text) {
    std::wstring result(text);
    for (auto& c : result) {
        c = static_cast<wchar_t>(std::towlower(c));
    }
    return result;
}

// Splits on commas and whitespace, dropping empty pieces.
std::vector<std::wstring> splitList(std::wstring_view text) {
    std::vector<std::wstring> parts;
    std::wstring current;
    for (wchar_t c : text) {
        if (c == L',' || c == L';' || std::iswspace(c)) {
            if (!current.empty()) {
                parts.push_back(lower(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(lower(current));
    }
    return parts;
}

// ---------------------------------------------------------------- install

enum class Status { Installed, Updated, UpToDate, Failed };

Status writeSkill(const fs::path& dir, std::span<const std::byte> content, std::wstring& error) {
    const fs::path file = dir / L"SKILL.md";
    std::error_code ec;
    const bool existed = fs::exists(file, ec);

    if (existed) {
        std::ifstream in(file, std::ios::binary);
        const std::string current{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        if (current.size() == content.size() &&
            std::memcmp(current.data(), content.data(), content.size()) == 0) {
            return Status::UpToDate;
        }
    }

    fs::create_directories(dir, ec);
    if (ec) {
        error = L"could not create the folder";
        return Status::Failed;
    }
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
        if (!out) {
            error = L"could not write SKILL.md";
            return Status::Failed;
        }
    }
    return existed ? Status::Updated : Status::Installed;
}

struct Candidate {
    const Tool* tool;
    std::optional<fs::path> evidence; // nullopt when the tool was not found
};

// Installs into the folders used by the chosen tools; each folder is written once.
int installInto(const std::vector<const Tool*>& chosen, const fs::path& home) {
    const auto content = embeddedSkill();
    if (content.empty()) {
        console::err(L"toaster: this build has no embedded skill\n");
        return toInt(ExitCode::Failure);
    }

    bool failed = false;
    console::out(L"\n");
    for (Folder folder : {Folder::Claude, Folder::Agents, Folder::Copilot}) {
        std::wstring users;
        for (const Tool* tool : chosen) {
            if (tool->folder == folder) {
                if (!users.empty()) {
                    users += L", ";
                }
                users += tool->name;
            }
        }
        if (users.empty()) {
            continue;
        }

        const fs::path dir = skillDirectory(folder, home);
        std::wstring error;
        const Status status = writeSkill(dir, content, error);
        std::wstring line = L"  ";
        switch (status) {
        case Status::Installed: line += L"installed   "; break;
        case Status::Updated:   line += L"updated     "; break;
        case Status::UpToDate:  line += L"up to date  "; break;
        case Status::Failed:    line += L"FAILED      "; failed = true; break;
        }
        line += (dir / L"SKILL.md").wstring();
        line += L"\n              for " + users;
        if (status == Status::Failed) {
            line += L" (" + error + L")";
        }
        line += L"\n";
        (status == Status::Failed ? console::err : console::out)(line);
    }

    if (failed) {
        return toInt(ExitCode::Failure);
    }
    console::out(L"\nDone. Restart the AI tool (or start a new session) so it picks up the skill.\n");
    return toInt(ExitCode::Ok);
}

std::vector<const Tool*> detectedTools(const std::vector<Candidate>& candidates) {
    std::vector<const Tool*> tools;
    for (const auto& candidate : candidates) {
        if (candidate.evidence) {
            tools.push_back(candidate.tool);
        }
    }
    return tools;
}

const Tool* findTool(std::wstring_view id) {
    for (const auto& tool : kTools) {
        if (tool.id == id) {
            return &tool;
        }
    }
    // A few friendly aliases.
    if (id == L"claude-code") return &kTools[0];
    if (id == L"code" || id == L"vs-code") return &kTools[3];
    if (id == L"visualstudio" || id == L"visual-studio") return &kTools[4];
    return nullptr;
}

std::wstring toolIds() {
    std::wstring ids;
    for (const auto& tool : kTools) {
        if (!ids.empty()) {
            ids += L", ";
        }
        ids += tool.id;
    }
    return ids;
}

// Non-interactive: --skill-target claude,codex  or  --skill-target all
int installTargets(std::wstring_view targets, const std::vector<Candidate>& candidates, const fs::path& home) {
    std::vector<const Tool*> chosen;
    for (const auto& part : splitList(targets)) {
        if (part == L"all") {
            for (const Tool* tool : detectedTools(candidates)) {
                if (std::ranges::find(chosen, tool) == chosen.end()) {
                    chosen.push_back(tool);
                }
            }
            continue;
        }
        const Tool* tool = findTool(part);
        if (!tool) {
            console::err(L"toaster: unknown skill target '" + part + L"' (expected all, " + toolIds() + L")\n");
            return toInt(ExitCode::UsageError);
        }
        if (std::ranges::find(chosen, tool) == chosen.end()) {
            chosen.push_back(tool);
        }
    }
    if (chosen.empty()) {
        console::err(L"toaster: no AI tools were found to install the skill into; name them with --skill-target "
                     L"(" + toolIds() + L")\n");
        return toInt(ExitCode::UsageError);
    }
    console::out(L"Installing the toaster skill:");
    return installInto(chosen, home);
}

// Interactive wizard.
int runWizard(const std::vector<Candidate>& candidates, const fs::path& home) {
    if (!console::stdinIsConsole()) {
        console::err(L"toaster: --install-skill asks questions and needs a console; in scripts use "
                     L"--skill-target <all|" + toolIds() + L">\n");
        return toInt(ExitCode::UsageError);
    }

    const auto detected = detectedTools(candidates);

    std::wstring screen = L"toaster skill installer\n\n"
                          L"Installs the 'toaster' agent skill, which teaches AI coding tools to notify you\n"
                          L"with toaster.exe when a task finishes, fails or needs your input.\n\n";
    screen += detected.empty() ? L"No AI developer tools were detected, but you can still pick any of these:\n\n"
                               : L"AI developer tools on this computer:\n\n";
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto& candidate = candidates[i];
        std::wstring name(candidate.tool->name);
        name.resize(std::max<std::size_t>(name.size(), 34), L' ');
        screen += L"  " + std::to_wstring(i + 1) + L") " + (candidate.evidence ? L"[found]     " : L"[not found] ") +
                  name + L"-> " + (skillDirectory(candidate.tool->folder, home)).wstring() + L"\n";
    }
    screen += L"\nCopilot CLI, the Copilot app, VS Code and Visual Studio share the .copilot folder.\n\n";
    console::out(screen);

    for (;;) {
        console::out(detected.empty()
                         ? L"Install into (numbers, e.g. 1,3; q to cancel): "
                         : L"Install into (numbers, e.g. 1,3; Enter or a = all found; q to cancel): ");
        std::wstring answer;
        if (!console::readLine(answer)) {
            console::out(L"\nCancelled.\n");
            return toInt(ExitCode::Ok);
        }
        const auto parts = splitList(answer);

        if (!parts.empty() && (parts[0] == L"q" || parts[0] == L"quit" || parts[0] == L"n" || parts[0] == L"no")) {
            console::out(L"Cancelled, nothing was installed.\n");
            return toInt(ExitCode::Ok);
        }

        std::vector<const Tool*> chosen;
        bool valid = true;
        if (parts.empty() || parts[0] == L"a" || parts[0] == L"all") {
            chosen = detected;
        } else {
            for (const auto& part : parts) {
                const Tool* tool = nullptr;
                if (part.size() <= 2 && std::ranges::all_of(part, [](wchar_t c) { return c >= L'0' && c <= L'9'; })) {
                    const std::size_t index = std::stoul(part);
                    if (index >= 1 && index <= candidates.size()) {
                        tool = candidates[index - 1].tool;
                    }
                } else {
                    tool = findTool(part);
                }
                if (!tool) {
                    console::out(L"  '" + part + L"' is not one of the choices.\n");
                    valid = false;
                    break;
                }
                if (std::ranges::find(chosen, tool) == chosen.end()) {
                    chosen.push_back(tool);
                }
            }
        }
        if (!valid) {
            continue;
        }
        if (chosen.empty()) {
            console::out(L"  Nothing selected; type the numbers of the tools you want.\n");
            continue;
        }
        return installInto(chosen, home);
    }
}

} // namespace

int print() {
    const auto content = embeddedSkill();
    if (content.empty()) {
        console::err(L"toaster: this build has no embedded skill\n");
        return toInt(ExitCode::Failure);
    }
    console::out(fromUtf8(content));
    return toInt(ExitCode::Ok);
}

int install(std::wstring_view targets) {
    const auto home = homeDirectory();
    if (!home) {
        console::err(L"toaster: could not find your user profile folder (USERPROFILE is not set)\n");
        return toInt(ExitCode::Failure);
    }

    std::vector<Candidate> candidates;
    candidates.reserve(kTools.size());
    for (const auto& tool : kTools) {
        candidates.push_back(Candidate{&tool, detect(tool, *home)});
    }

    return targets.empty() ? runWizard(candidates, *home) : installTargets(targets, candidates, *home);
}

} // namespace toaster::skill
