# toaster

A small, fast command-line tool for Windows 11 that shows toast notifications
and plays `.wav` files. It ships as a single self-contained `toaster.exe`:
drop it anywhere on your `PATH`.

> Work in progress: the command line and asset embedding are in place;
> showing toasts and playing sounds are coming next.

## Usage

```
toaster -t "Build done" -b "All green" -i logo.png -s done.wav --wait
```

| Short | Long              | Meaning                                                   |
|-------|-------------------|-----------------------------------------------------------|
| `-t`  | `--title <text>`  | Toast title (required)                                    |
| `-b`  | `--body <text>`   | Toast body text                                           |
| `-i`  | `--image <path\|name>` | Image file, or the name of an embedded image         |
| `-s`  | `--sound <path\|name>` | `.wav` file, or the name of an embedded sound        |
| `-w`  | `--wait`          | Block until the toast is dismissed or clicked             |
|       | `--timeout <sec>` | With `--wait`: give up after this many seconds            |
|       | `--list-embedded` | List sounds and images compiled into the `.exe`           |
| `-h`  | `--help`          | Show help                                                 |
| `-v`  | `--version`       | Show version                                              |

Values can also be written as `--title=value`. By default toaster shows the
notification and returns immediately.

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
