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
toaster -t "Build finished" -b "All 214 tests passed" -i success
toaster -t "Build failed" -b "3 errors in parser.cpp" -i failure --pack peon -c task.error
toaster -t "Need your input" -b "Which database should I migrate?" -i question --pack peon -c input.required
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
|       | `--list-categories`     | With `--pack`: list that pack's categories, one per line |
| `-h`  | `--help`                | Full help |

Values can also be written as `--title=value`. Always quote text that contains
spaces.

Title and body are both optional, but give at least one of them to show a
toast. With neither, toaster shows nothing and only plays the sound, so
`-s`/`--sound` is then required:

```
toaster -s peon_peonyes1          # sound only, no toast
```

## Sounds, images and packs

A standard toaster.exe has its sounds and icons compiled in, so there is
nothing to install and no file paths to pass. Refer to them by name. Run
`toaster --list-embedded` and `toaster --list-packs` once if you need the exact
list for this build. A path to a real file works too.

### Icons (`-i <name>`)

Embedded icons are named after the event. Pick the one that matches:

| Event | Icon |
|-------|------|
| Task succeeded | `success` (big win: `celebrate`, `trophy`) |
| Task failed | `failure` (tests: `bug`, crash: `crash`, outage: `fire`) |
| Need the user's input or approval | `question` |
| Warning | `warning` |
| Blocked or stopped | `stop` |
| Information or tip | `info`, `idea` |
| Build, deploy, install | `build`, `deploy`, `package` |
| Still running, long wait, finished | `waiting`, `timer`, `coffee`, `done` |
| Approved or rejected | `thumbs_up`, `thumbs_down` |
| Security or permissions | `locked` |
| Other | `bell` (generic), `robot` (AI agent), `alarm`, `save`, `settings`, `computer`, `mail`, `sparkles` |

Use `-i` with `--image-style hero` only if the user asks for a large banner;
the default `logo` is right for almost everything.

### Sound packs (`--pack <name> --category <category>`)

Sounds come in packs. `--pack` plays a random sound from the category, and the
sound's text label becomes the body and the pack's name the title, unless you
pass `-t`/`-b`. The packs have no icon of their own, so always add `-i`.

| Pack | Voice | Language |
|------|-------|----------|
| `peon` | Orc Peon (Warcraft) | English |
| `glados` | GLaDOS (Portal) | English |
| `jan_pawel_2` | Jan Pawel 2 | Polish |

Categories, in every pack: `task.complete` (success), `task.error` (failure),
`input.required` (you need the user), `task.acknowledge` (started working),
`session.start`, `resource.limit` (quota, disk, rate limit) and `user.spam`.
`jan_pawel_2` also has `session.end` and `task.progress`.

```
toaster --pack peon -c task.complete -t "Build finished" -b "214 tests passed" -i success
toaster --pack glados -c task.error -t "Tests failed" -b "See test.log" -i failure
toaster --pack peon -c input.required -t "Need your input" -b "Which database?" -i question
```

Use the user's usual pack if they named one (or one is set in their
instructions); otherwise use `peon`. Match category and icon to the event and
don't mix them, e.g. no `task.error` sound with `-i success`.

A single pack sound can also be played by name as `<pack>_<file name>`, for
example `-s peon_peonready1`. Run `toaster --list-embedded` for the names.

## Fire-and-forget vs. waiting

By default toaster returns right away (or once the sound has finished). Prefer
this: never block your own work on a notification.

`--wait` blocks until the user reacts. Use it only when the user asked for a
click-to-confirm flow, and always add `--timeout` so you can't hang forever:

```
toaster -t "Deploy to staging?" -b "Click to confirm" -i question --wait --timeout 120
```

To ask a question, add buttons with `-B`/`--button` (needs `--wait`). The toast
then stays on screen until answered; toaster prints the clicked label to stdout
and exits with 11 for the first button, 12 for the second, and so on:

```
toaster -t "Deploy to staging?" -i question -B Deploy -B Cancel --wait --timeout 120
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
