# Agent: Fix VST3 SDK Include Path in Architecture Check

You are working on the `feature/plugin-scan-bar` branch of Drem Canvas, a C++17 DAW
using Skia for rendering. Your task is to fix the `check_architecture.sh` script so
that dc:: header self-containment checks pass for headers that include VST3 SDK types.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `scripts/check_architecture.sh` (the failing check — see Check 2 starting at line 91)
- `src/dc/plugins/ComponentHandler.h` (example failing header — includes `<pluginterfaces/vst/ivsteditcontroller.h>`)
- `CMakeLists.txt` (search for `VST3_SDK_DIR` and `vst3sdk` to see how the SDK is fetched)

## Problem

The header self-containment check (Check 2) compiles each `src/dc/*.h` file with:

```bash
c++ -std=c++17 -fsyntax-only -I . -I src/ -I build/generated/ -x c++ <tmpfile>
```

Four dc:: plugin headers include `<pluginterfaces/...>` from the VST3 SDK, which
lives in `build/_deps/vst3sdk-src/` (fetched by CMake FetchContent). The check
doesn't include this path, so they all fail:

```
src/dc/plugins/ComponentHandler.h
src/dc/plugins/PluginEditor.h
src/dc/plugins/PluginInstance.h
src/dc/plugins/VST3Host.h
```

The script already handles similar external dependencies (portaudio, rtmidi) by
detecting them via `pkg-config` and adding appropriate flags. The VST3 SDK is
different — it's fetched into the build directory, not installed system-wide.

## Deliverables

### Modified file: `scripts/check_architecture.sh`

Add VST3 SDK detection and include path handling, following the same pattern as
portaudio and rtmidi. Specifically:

#### 1. Detect the VST3 SDK path

After the existing `RTMIDI_FLAGS` detection block (around line 108), add detection
for the VST3 SDK. Check multiple possible build directories since the project
supports different presets:

```bash
VST3_SDK_DIR=""
for build_dir in build build-debug build-coverage; do
    candidate="$build_dir/_deps/vst3sdk-src"
    if [ -d "$candidate/pluginterfaces" ]; then
        VST3_SDK_DIR="$candidate"
        break
    fi
done
```

#### 2. Add VST3 flags to the per-header include logic

In the `case` block that matches headers to extra flags (around line 116), add a
case for plugin headers. Match any header under `dc/plugins/` that includes VST3
SDK headers:

```bash
*dc/plugins/ComponentHandler* | *dc/plugins/PluginEditor* | \
*dc/plugins/PluginInstance* | *dc/plugins/VST3Host*)
    if [ -n "$VST3_SDK_DIR" ]; then
        EXTRA_FLAGS="-I $VST3_SDK_DIR"
    else
        NEEDS_SYSTEM_LIB="vst3sdk (run cmake --preset release first)"
    fi
    ;;
```

This approach:
- Skips these headers with a helpful message if no build has been run yet
- Includes the SDK path when available (after any cmake configure)
- Follows the same pattern as the portaudio/rtmidi handling

#### 3. Verify the fix

After making changes, run:

```bash
scripts/check_architecture.sh --check 2
```

Expected output:
```
Check 2: dc:: header self-containment ... OK
```

If portaudio is not installed, it should show:
```
Check 2: dc:: header self-containment ... OK
  (skipped due to missing system libraries:
  src/dc/audio/PortAudioDeviceManager.h (missing portaudio))
```

Also run the full check:
```bash
scripts/check_architecture.sh
```

Both checks should pass.

## Conventions

- Shell style: POSIX sh (no bashisms — script uses `#!/bin/sh`)
- Variable naming: UPPER_SNAKE_CASE for script-level variables
- Keep the existing comment style and section structure
- Build verification: `cmake --build --preset release`
