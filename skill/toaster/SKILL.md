---
name: toaster
description: Show a Windows toast notification (with optional sound and image) using toaster.exe to tell the user something needs their attention. Use when a long-running task finishes or fails, when you are blocked and need the user's input or approval, or when the user asks to be notified, pinged or alerted. Windows only; needs toaster.exe on PATH.
---

# toaster: Windows toast notifications from the command line

`toaster.exe` is a small Windows 11 command-line tool that shows a toast
notification and can play a `.wav` or `.mp3` at the same time. Use it to get
the user's attention when they are probably looking at another window.

## When to notify

- A long task finished (build, test run, install, migration, long refactor).
- A task failed and you can't continue on your own.
- You are blocked and need the user to answer a question or approve a step.
- The user explicitly asked to be notified.

Do not notify for every small step. One toast per meaningful event; keep the
title short (what happened) and put details in the body (one or two lines).

## Check it is available

```
toaster --version
```

If the command is not found, toaster is not installed or not on `PATH`; tell
the user once and carry on without notifications.

## Basic usage

```
toaster -t "Build finished" -b "All 214 tests passed"
toaster -t "Build failed" -b "3 errors in parser.cpp" -s <sound>
toaster -t "Need your input" -b "Which database should I migrate?" -i <image>
```

| Short | Long                    | Meaning |
|-------|-------------------------|---------|
| `-t`  | `--title <text>`        | Toast title (optional) |
| `-b`  | `--body <text>`         | Body text (optional) |
| `-i`  | `--image <path\|name>`  | Image file, or the name of an embedded image |
|       | `--image-style <style>` | `logo` (default), `inline` or `hero` |
| `-s`  | `--sound <path\|name>`  | `.wav`/`.mp3` file, or the name of an embedded sound |
| `-p`  | `--pack <name>`         | Sound pack: plays a random sound from `--category` |
| `-c`  | `--category <name>`     | Pack category, e.g. `task.complete`, `task.error`, `input.required` |
| `-B`  | `--button <label>`      | Add an action button (repeatable, up to 5; needs `--wait`) |
| `-w`  | `--wait`                | Block until the toast is clicked or dismissed |
|       | `--timeout <sec>`       | With `--wait`: give up after this many seconds |
|       | `--list-embedded`       | List sounds and images built into this toaster.exe |
|       | `--list-packs`          | List built-in sound packs and their categories |
| `-h`  | `--help`                | Full help |

Values can also be written as `--title=value`. Always quote text that contains
spaces.

Title and body are both optional, but give at least one of them to show a
toast. With neither, toaster shows nothing and only plays the sound, so
`-s`/`--sound` is then required:

```
toaster -s done          # sound only, no toast
```

## Sounds, images and packs

Each toaster.exe can have sounds, images and sound packs compiled in. Run
`toaster --list-embedded` and `toaster --list-packs` once to see what this
build offers, then refer to them by name (`-s done`, `-i logo`). A path to a
real file works too.

With `--pack`, the title, body and image default to the pack's own values, so
this is the shortest way to make a noise:

```
toaster --pack <pack> --category task.complete
toaster --pack <pack> -c task.error -t "Tests failed" -b "See test.log"
```

Pick the category that matches the event when the pack has it:
`task.complete` for success, `task.error` for failures, `input.required` when
you need the user. Fall back to plain `-t`/`-b` if there are no packs.

## Fire-and-forget vs. waiting

By default toaster returns right away (or once the sound has finished). Prefer
this: never block your own work on a notification.

`--wait` blocks until the user reacts. Use it only when the user asked for a
click-to-confirm flow, and always add `--timeout` so you can't hang forever:

```
toaster -t "Deploy to staging?" -b "Click to confirm" --wait --timeout 120
```

To ask a question, add buttons with `-B`/`--button` (needs `--wait`). The toast
then stays on screen until answered; toaster prints the clicked label to stdout
and exits with 11 for the first button, 12 for the second, and so on:

```
toaster -t "Deploy to staging?" -B Deploy -B Cancel --wait --timeout 120
```

Only proceed on the exact "yes" button (exit 11 here). Treat every other code,
including 3 (closed) and 5 (timeout), as "no".

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Shown (with `--wait`: the user clicked it) |
| 1 | Bad arguments, or the image/sound was not found |
| 2 | Windows failed to show it (for example, notifications are turned off) |
| 3 | `--wait`: the user closed the toast |
| 4 | `--wait`: the toast expired to Action Center |
| 5 | `--wait --timeout`: time ran out |
| 11–15 | `--button`: button 1–5 was clicked (label printed to stdout) |

A non-zero code from a plain notification is not a reason to stop your task;
report it to the user only if they rely on the toasts.

## Shell notes

- PowerShell: `toaster -t "Done" -b "Backup of $env:COMPUTERNAME finished"`,
  then check `$LASTEXITCODE`.
- CMD: `toaster -t "Done" --wait` then `if %ERRORLEVEL% EQU 0 ...`.
- Git Bash / WSL: call `toaster.exe` (with the extension from WSL). Quote with
  double quotes; avoid unescaped `!` in bash.
- Keep text plain: no Markdown, no ANSI colours. Long bodies are cut off by
  Windows, so stay under ~200 characters.
