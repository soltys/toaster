# toaster

A small, fast command-line tool for Windows 11 that shows toast notifications
and plays `.wav` files. It ships as a single self-contained `toaster.exe`:
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
| `-t`  | `--title <text>`  | Toast title (required)                                    |
| `-b`  | `--body <text>`   | Toast body text                                           |
| `-i`  | `--image <path\|name>` | Image file, or the name of an embedded image         |
|       | `--image-style <style>` | `logo` (small, next to the text; default), `inline` or `hero` |
| `-s`  | `--sound <path\|name>` | `.wav` file, or the name of an embedded sound        |
| `-w`  | `--wait`          | Block until the toast is dismissed or clicked             |
|       | `--timeout <sec>` | With `--wait`: give up after this many seconds            |
|       | `--list-embedded` | List sounds and images compiled into the `.exe`           |
| `-h`  | `--help`          | Show help                                                 |
| `-v`  | `--version`       | Show version                                              |

Values can also be written as `--title=value`.

By default toaster shows the notification and returns immediately. When a
sound is given it returns as soon as the sound has finished playing; the
`.wav` replaces the default Windows notification sound.

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

Requires Visual Studio 2022 (MSVC, C++20) with the Windows 10/11 SDK, and CMake 3.21+.

```
cmake --preset msvc-x64
cmake --build --preset release
```

The result is `build/msvc-x64/Release/toaster.exe`, statically linked against
the C/C++ runtime.

### Embedding sounds and images

Any number of `.wav` files and images (`.png`, `.jpg`, `.jpeg`, `.gif`) can be
compiled into the executable:

```
cmake --preset msvc-x64 ^
  -DTOASTER_EMBED_SOUNDS="assets/sounds/done.wav;assets/sounds/fail.wav" ^
  -DTOASTER_EMBED_IMAGES="assets/images/logo.png"
cmake --build --preset release
```

Each asset is referenced by its file name without the extension, lower-cased,
with anything other than letters, digits and `_` turned into `_`
(`Build-Done.wav` becomes `build_done`):

```
toaster -t "Build done" -s done -i logo
```

If a file with that name exists in the current directory it is used instead.
Windows only shows toast images from files, so embedded images are unpacked once
to `%LOCALAPPDATA%\toaster\cache`.
