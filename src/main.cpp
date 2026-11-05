#include "pch.h"

#include "cli.h"
#include "console.h"
#include "embedded.h"
#include "exit_code.h"
#include "packs.h"
#include "skill.h"
#include "sound.h"
#include "toast.h"
#include "version.h"

namespace {

using toaster::ExitCode;

int toInt(ExitCode code) { return static_cast<int>(code); }

// Thrown for problems with the user's input (missing image, bad .wav, ...).
struct InputError {
    std::wstring message;
};

bool isRegularFile(const std::wstring& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

std::filesystem::path resolveImage(const std::wstring& value) {
    if (isRegularFile(value)) {
        std::error_code ec;
        auto absolute = std::filesystem::absolute(value, ec);
        return ec ? std::filesystem::path(value) : absolute;
    }
    if (auto extracted = toaster::embedded::extractImage(value)) {
        return *extracted;
    }
    throw InputError{L"image not found: '" + value + L"' (not a file and not an embedded image)"};
}

toaster::sound::Source resolveSound(const std::wstring& value) {
    using namespace toaster;
    if (isRegularFile(value)) {
        const auto format = sound::detectFormat(value);
        if (!format) {
            throw InputError{L"not a .wav or .mp3 file: '" + value + L"'"};
        }
        return sound::FileSound{value, *format};
    }
    if (const auto embeddedSound = embedded::findSound(value)) {
        const auto format =
            embeddedSound->format == embedded::SoundFormat::Mp3 ? sound::Format::Mp3 : sound::Format::Wav;
        return sound::EmbeddedSound{embeddedSound->resourceName, format};
    }
    throw InputError{L"sound not found: '" + value + L"' (not a file and not an embedded sound)"};
}

toaster::toast::ImageStyle toImageStyle(std::wstring_view style) {
    if (style == L"inline") return toaster::toast::ImageStyle::Inline;
    if (style == L"hero") return toaster::toast::ImageStyle::Hero;
    return toaster::toast::ImageStyle::Logo;
}

ExitCode toExitCode(toaster::toast::Outcome outcome) {
    using toaster::toast::Outcome;
    switch (outcome) {
    case Outcome::Pending:
    case Outcome::Activated: return ExitCode::Ok;
    case Outcome::ButtonClicked: return ExitCode::FirstButton; // offset added by the caller
    case Outcome::Dismissed: return ExitCode::Dismissed;
    case Outcome::Expired:   return ExitCode::Expired;
    case Outcome::TimedOut:  return ExitCode::TimedOut;
    case Outcome::Failed:    return ExitCode::Failure;
    }
    return ExitCode::Failure;
}

// Fills in --pack defaults: a random sound from the category, and the pack's
// display name, the sound's label and the pack icon where the user gave none.
toaster::cli::Options applyPack(toaster::cli::Options options) {
    using namespace toaster;
    const packs::SoundPack* pack = packs::find(options.pack);
    if (!pack) {
        throw InputError{L"unknown sound pack: '" + options.pack + L"' (see --list-packs)"};
    }
    const packs::PackCategory* category = packs::findCategory(*pack, options.category);
    if (!category) {
        throw InputError{L"sound pack '" + std::wstring(pack->name) + L"' has no category '" + options.category +
                         L"' (available: " + packs::categoryNames(*pack) + L")"};
    }
    const packs::PackSound& picked = packs::pickRandom(*category);
    options.sound.assign(picked.asset);
    if (options.title.empty()) {
        options.title.assign(pack->displayName);
    }
    if (options.body.empty()) {
        options.body.assign(picked.label);
    }
    if (options.image.empty()) {
        options.image.assign(pack->iconAsset);
    }
    return options;
}

int notify(const toaster::cli::Options& requested) {
    const toaster::cli::Options options = requested.pack.empty() ? requested : applyPack(requested);

    toaster::toast::Content content;
    content.title = options.title;
    content.body = options.body;
    content.imageStyle = toImageStyle(options.imageStyle);
    content.buttons = options.buttons;
    if (!options.image.empty()) {
        content.image = resolveImage(options.image);
    }

    std::optional<toaster::sound::Source> sound;
    if (!options.sound.empty()) {
        sound = resolveSound(options.sound);
        content.silent = true; // our sound replaces the Windows notification sound
    }

    if (options.title.empty() && options.body.empty()) {
        // Sound-only mode: no toast, play the sound to the end and exit.
        std::wstring error;
        if (!toaster::sound::play(*sound, /*async=*/false, error)) {
            toaster::console::err(L"toaster: could not play '" + options.sound + L"': " + error + L"\n");
            return toInt(ExitCode::Failure);
        }
        return toInt(ExitCode::Ok);
    }

    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    auto notification = toaster::toast::Notification::show(content, options.wait);

    std::unique_ptr<toaster::sound::Playback> playback; // stops the sound when destroyed
    if (sound) {
        // Fire-and-forget: play synchronously so the process lives exactly as
        // long as the sound. With --wait: play in the background while waiting.
        std::wstring error;
        playback = toaster::sound::play(*sound, /*async=*/options.wait, error);
        if (!playback) {
            toaster::console::err(L"toaster: warning: could not play '" + options.sound + L"': " + error + L"\n");
        }
    }

    if (!options.wait) {
        return toInt(ExitCode::Ok);
    }

    std::optional<std::chrono::milliseconds> timeout;
    if (options.timeoutSeconds) {
        timeout = std::chrono::seconds(*options.timeoutSeconds);
    }
    const auto outcome = notification->wait(timeout);
    if (outcome == toaster::toast::Outcome::Failed) {
        wchar_t code[16];
        swprintf_s(code, L"0x%08X", static_cast<unsigned>(notification->errorCode()));
        toaster::console::err(std::wstring(L"toaster: Windows failed to show the notification (") + code + L")\n");
    }
    if (outcome == toaster::toast::Outcome::ButtonClicked) {
        const std::size_t button = notification->clickedButton(); // 1-based
        if (button >= 1 && button <= options.buttons.size()) {
            toaster::console::out(options.buttons[button - 1] + L"\n");
            return toInt(ExitCode::FirstButton) + static_cast<int>(button) - 1;
        }
        return toInt(ExitCode::Ok);
    }
    return toInt(toExitCode(outcome));
}

int run(const toaster::cli::Options& options) {
    if (options.help) {
        toaster::console::out(toaster::cli::usage());
        return toInt(ExitCode::Ok);
    }
    if (options.version) {
        toaster::console::out(std::wstring(L"toaster ") + TOASTER_VERSION + L"\n");
        return toInt(ExitCode::Ok);
    }
    if (options.listEmbedded) {
        toaster::console::out(toaster::embedded::describe());
        return toInt(ExitCode::Ok);
    }
    if (options.printSkill) {
        return toaster::skill::print();
    }
    if (options.installSkill) {
        return toaster::skill::install(options.skillTarget);
    }
    if (options.listPacks) {
        toaster::console::out(toaster::packs::describe());
        return toInt(ExitCode::Ok);
    }
    if (options.listCategories) {
        const toaster::packs::SoundPack* pack = toaster::packs::find(options.pack);
        if (!pack) {
            toaster::console::err(L"toaster: unknown sound pack: '" + options.pack + L"' (see --list-packs)\n");
            return toInt(ExitCode::UsageError);
        }
        toaster::console::out(toaster::packs::categoryLines(*pack));
        return toInt(ExitCode::Ok);
    }

    try {
        return notify(options);
    } catch (const InputError& error) {
        toaster::console::err(L"toaster: " + error.message + L"\n");
        return toInt(ExitCode::UsageError);
    } catch (const winrt::hresult_error& error) {
        wchar_t code[16];
        swprintf_s(code, L"0x%08X", static_cast<unsigned>(error.code().value));
        toaster::console::err(L"toaster: " + std::wstring(error.message()) + L" (" + code + L")\n");
        return toInt(ExitCode::Failure);
    }
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::wstring_view> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const auto parsed = toaster::cli::parse(args);
    if (const auto* error = std::get_if<toaster::cli::ParseError>(&parsed)) {
        toaster::console::err(L"toaster: " + error->message + L"\n" +
                              L"Run 'toaster --help' for usage.\n");
        return toInt(ExitCode::UsageError);
    }

    return run(std::get<toaster::cli::Options>(parsed));
}
