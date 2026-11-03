# toaster

A small, fast command-line tool for Windows 11 that shows toast notifications
and plays `.wav` or `.mp3` files. It ships as a single self-contained `toaster.exe`:
drop it anywhere on your `PATH`.

Toasts are sent under the app ID `Soltys.Toaster`, which toaster registers in
`HKCU\Software\Classes\AppUserModelId` on first use. No installer, no admin
rights and no Start Menu shortcut are needed.

## Usage

```
toaster -t "Build done" -b "All green" -i logo.png -s done.wav --wait
```

| Short | Long              | Meaning                                                   |
|-------|-------------------|-----------------------------------------------------------|
| `-t`  | `--title <text>`  | Toast title (optional)                                    |
| `-b`  | `--body <text>`   | Toast body text                                           |
| `-i`  | `--image <path\|name>` | Image file, or the name of an embedded image         |
|       | `--image-style <style>` | `logo` (small, next to the text; default), `inline` or `hero` |
| `-s`  | `--sound <path\|name>` | `.wav`/`.mp3` file, or the name of an embedded sound |
| `-p`  | `--pack <name>`   | Sound pack: play a random sound from `--category`         |
| `-c`  | `--category <name>` | Pack category, e.g. `task.complete`                     |
| `-w`  | `--wait`          | Block until the toast is dismissed or clicked             |
|       | `--timeout <sec>` | With `--wait`: give up after this many seconds            |
|       | `--list-embedded` | List sounds and images compiled into the `.exe`           |
|       | `--list-packs`    | List embedded sound packs, their categories and sounds    |
| `-h`  | `--help`          | Show help                                                 |
| `-v`  | `--version`       | Show version                                              |

Values can also be written as `--title=value`.

By default toaster shows the notification and returns immediately. When a
sound is given it returns as soon as the sound has finished playing; the
sound replaces the default Windows notification sound.

With `--wait` it blocks until the toast is resolved. Windows moves an unclicked
toast to Action Center after a few seconds, which also ends the wait.

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | Toast shown (with `--wait`: the user clicked it) |
| 1 | Bad arguments, or the image/sound could not be found |
| 2 | Windows refused or failed to show the toast (for example, notifications are turned off) |
| 3 | `--wait`: the user closed the toast |
| 4 | `--wait`: the toast left the screen on its own |
| 5 | `--wait --timeout`: time ran out and the toast was removed |

### PowerShell

```powershell
toaster -t "Backup finished" -b "$env:COMPUTERNAME" -s done.wav
toaster -t "Deploy?" -b "Click to confirm" --wait --timeout 60
if ($LASTEXITCODE -eq 0) { ./deploy.ps1 }
```

### CMD

```bat
toaster -t "Build failed" -b "See build.log" -i error.png -s fail.wav
toaster -t "Ready" --wait
if %ERRORLEVEL% EQU 3 echo Closed by the user
```

## Building

Requires Visual Studio 2022 (MSVC, C++20) with the Windows 10/11 SDK, and CMake 4.2.3+.

```
cmake --preset msvc-x64
cmake --build --preset release
```

The result is `build/msvc-x64/Release/toaster.exe`, statically linked against
the C/C++ runtime.

### Embedding sounds and images

Every `.wav` and `.mp3` file in `assets/sounds` and every image (`.png`, `.jpg`, `.jpeg`,
`.gif`) in `assets/images` is compiled into the executable. Drop files into
those folders and rebuild; CMake notices added or removed files on its own.

To use other folders (relative paths are resolved against the repo root):

```
cmake --preset msvc-x64 -DTOASTER_SOUNDS_DIR=C:/my/sounds -DTOASTER_IMAGES_DIR=C:/my/images
cmake --build --preset release
```

Pass an empty value (`-DTOASTER_IMAGES_DIR=`) to embed nothing of that kind.
Subfolders are not scanned.

Each asset is referenced by its file name without the extension, lower-cased,
with anything other than letters, digits and `_` turned into `_`
(`Build-Done.wav` becomes `build_done`):

```
toaster --list-embedded
toaster -t "Build done" -s done -i logo
```

If a file with that name exists in the current directory it is used instead.
Windows only shows toast images from files and plays MP3 (through MCI) only
from files, so embedded images and `.mp3` sounds are unpacked once to
`%LOCALAPPDATA%\toaster\cache`. Embedded `.wav` files play straight from the `.exe`.

### Sound packs

The sounds folder can also hold sound pack descriptions: JSON files in the
CESP format (`"cesp_version": "1.0"`), for example `peon.json` or
`glados.json`. Paths inside a pack (`categories.<name>.sounds[].file` and the
optional `icon`) are looked up relative to the JSON file, then by file name in
the JSON folder, in a `<pack>/` subfolder, and (icons only) in the images folder.
Missing files are skipped with a warning. Typical layout:

```
sounds/
  peon.json
  glados.json
  GLaDOS_P2.png
  sounds/
    PeonReady1.wav
    Hello.mp3
    ...
```

CMake reads the packs when it configures the build: it checks every file's
`sha256` (a mismatch stops the build), embeds the sounds and the icon, and
compiles the pack names, categories and labels into the `.exe`. Nothing is
parsed at runtime.

```
toaster --list-packs
toaster --pack peon --category task.complete
toaster --pack glados -c task.error -t "Build failed" --wait
```

`--pack` plays a random sound from the category. Unless you pass them yourself,
the title is the pack's `display_name`, the body is the sound's `label` and the
image is the pack's `icon`. Pack sounds can also be played directly as
`<pack>_<file name>`, for example `-s peon_peonready1`.
