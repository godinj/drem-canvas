# Drem Canvas — DAW Project

## Build

First-time setup (installs dependencies, builds Skia, configures CMake):

```bash
scripts/bootstrap.sh
```

Build using CMake presets:

```bash
cmake --build --preset release
```

If the build directory doesn't exist or you want to reconfigure:

```bash
cmake --preset release          # configure (Ninja + Release)
cmake --build --preset release  # build
```

Debug build:

```bash
cmake --preset debug
cmake --build --preset debug
```

Check dependency status:

```bash
scripts/check_deps.sh
```

Run (macOS):

```bash
open "build/DremCanvas_artefacts/Release/Drem Canvas.app"
```

Run (Linux):

```bash
./build/DremCanvas_artefacts/Release/DremCanvas
```

### Linux Dependencies

Install before running `scripts/bootstrap.sh`:

Debian/Ubuntu:
```bash
sudo apt install cmake ninja-build python3 libpng-dev \
    libvulkan-dev libglfw3-dev libfontconfig-dev libasound2-dev
```

Fedora:
```bash
sudo dnf install cmake ninja-build python3 libpng-devel \
    vulkan-devel glfw-devel fontconfig-devel alsa-lib-devel
```

### Wine Version Requirement (VST Plugins)

**Wine must be pinned to version 9.21.** Wine >= 9.22 introduces a regression that
breaks mouse coordinate handling in yabridge-bridged VST plugin UIs. Clicks land at
wrong positions (offset by the plugin window's screen coordinates).

Root cause: Wine 9.22 refactored the X11 window state/ConfigureNotify tracker
(Wine MR !6569). The new code ignores ConfigureNotify events that report the plugin
window's screen position, so Wine assumes position (0,0). Win32 `ScreenToClient()`
inside the plugin then produces coordinates offset by the window's actual position.

Tracking:
- yabridge issue: https://github.com/robbert-vdh/yabridge/issues/382
- yabridge fix: PR #405 merged into `new-wine10-embedding` branch (not yet released)
- Wine change: MR !6569 (first shipped in Wine 9.22)

To pin Wine 9.21 on Debian/Ubuntu:
```bash
sudo apt install winehq-staging=9.21~trixie-1 \
    wine-staging=9.21~trixie-1 \
    wine-staging-amd64=9.21~trixie-1 \
    wine-staging-i386:i386=9.21~trixie-1
sudo apt-mark hold winehq-staging wine-staging wine-staging-amd64 wine-staging-i386
```

Remove the hold once yabridge releases a version with the fix:
```bash
sudo apt-mark unhold winehq-staging wine-staging wine-staging-amd64 wine-staging-i386
```

## Architecture

- **C++17** with **JUCE 8** framework, **yaml-cpp** for serialization
- **GPU rendering**: Skia + Metal (macOS) / Skia + Vulkan (Linux)
- **Windowing**: Native Cocoa/MTKView (macOS) / GLFW 3 (Linux)
- Namespace: `dc`
- `src/engine/` — Real-time audio (never allocates or locks on audio thread)
- `src/model/` — Data model using JUCE `ValueTree` (message thread only)
- `src/gui/` — Presentational components observing model state
- `src/vim/` — Vim modal navigation (KeyListener intercepting before child widgets)
- `src/plugins/` — VST3/AU plugin hosting (AU on macOS only)
- `src/utils/` — Helpers (undo, audio file utils)
- `src/platform/` — macOS native layer (`.mm` files)
- `src/platform/linux/` — Linux platform layer (GLFW, Vulkan, zenity dialogs)
- `src/graphics/rendering/GpuBackend.h` — Abstract GPU interface (MetalBackend / VulkanBackend)

### JUCE Submodule Patches

The JUCE submodule (`libs/JUCE`) has local patches on top of upstream commit `501c076`
that add IParameterFinder spatial hints and performEdit snoop support for VST3 plugin
parameter discovery. These patches are:

- **Canonical source**: `scripts/juce-patches/*.patch`
- **Applied by**: `scripts/bootstrap.sh` (automatic on new worktrees)
- **Affected files**: `juce_VST3PluginFormat.cpp`, `juce_VST3PluginFormatImpl.h`

Each worktree has its own JUCE submodule git directory (objects are NOT shared).
`bootstrap.sh` handles this automatically — it fetches patch commits from sibling
worktrees when possible, or applies the `.patch` files as a fallback.

**If spatial hints stop working** (overlay shows 0 results for all plugins), the JUCE
patches are likely missing. Run `scripts/bootstrap.sh` to re-apply them.

## Key Patterns

- `ValueTree` is the single source of truth for all model state
- GUI components observe ValueTree changes via `ValueTree::Listener`
- Audio thread communicates with GUI via `std::atomic` and lock-free FIFOs
- `VimEngine` is a `juce::KeyListener` attached to `MainComponent` — intercepts all keys in Normal mode, passes through in Insert mode
- Track selection lives in `Arrangement`, clip selection in `VimContext`
- `Project::getUndoManager()` provides the shared `juce::UndoManager`

## Playhead & Timeline Coordinate System

All timeline positions (playhead, clips, markers) follow one coordinate transformation chain. Every component that draws on the timeline MUST use this same math to stay aligned.

**Conversion chain:**
```
SAMPLES  (TransportController::getPositionInSamples(), clip startPosition/length)
   ÷ sampleRate
SECONDS  (logical timeline position)
   × pixelsPerSecond
TIMELINE PIXELS  (absolute pixel offset from time=0)
   + headerWidth (150px)
   - scrollOffset (viewport.getViewPositionX())
SCREEN PIXELS  (on-screen x coordinate)
```

**Canonical formulas:**
```cpp
// Playhead screen position (ArrangementView::paint)
float cursorX = float(posInSamples / sampleRate * pixelsPerSecond)
              + 150.0f - float(viewport.getViewPositionX());

// Clip bounds (TrackLane::resized)
int x = roundToInt((startPosition / sampleRate) * pixelsPerSecond) + headerWidth;
int w = roundToInt((length / sampleRate) * pixelsPerSecond);

// TimeRuler tick position
float x = float((time - scrollOffset / pixelsPerSecond) * pixelsPerSecond) + headerWidth;

// Mouse click to time (TimeRuler seek)
double timeInSeconds = (double(mouseX - headerWidth) + scrollOffset) / pixelsPerSecond;
```

**Shared constants (must stay in sync across components):**
- `headerWidth = 150` — defined in `TrackLane` and `TimeRuler` (track name column width)
- `pixelsPerSecond = 100.0` — zoom level, stored in `ArrangementView` and propagated to `TrackLane`/`TimeRuler`
- `sampleRate` — from `TransportController::getSampleRate()` or `Project::getSampleRate()`

**Rules:**
- All positions in the model (`startPosition`, `length`, `trimStart`, `trimEnd`) are in **samples**, never seconds or pixels
- `TransportController` stores position as `std::atomic<int64_t>` samples — safe to read from any thread
- Conversion to seconds/pixels happens only in GUI code at draw time
- The playhead is drawn in `ArrangementView::paint()` as a red vertical line (not using the `Cursor` component)
- `ArrangementView` repaints at 30Hz via `juce::Timer` to animate the playhead
- When adding new timeline-aware components, always derive screen position using the full chain above — do not skip the scroll offset or header width

## Conventions

- JUCE coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use `<JuceHeader.h>` plus project-relative paths (e.g., `"model/Project.h"`)
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Always verify with `cmake --build --preset release` after changes

## Verification

1. `scripts/bootstrap.sh` succeeds when run twice (idempotent)
2. Clean build: delete `build/`, run `cmake --preset release && cmake --build --preset release`
3. `scripts/check_deps.sh` exits 0 with all `[OK]`
4. In worktree context, `ls -la libs/skia` shows symlink to shared cache
5. `libs/JUCE/CMakeLists.txt` exists
6. macOS: App launches without crash: `open "build/DremCanvas_artefacts/Release/Drem Canvas.app"`
7. Linux: App launches without crash: `./build/DremCanvas_artefacts/Release/DremCanvas`

## Current State

Phases 1-4 and 8-10 implemented:
- Audio engine with multi-track playback and recording
- Arrangement view with waveform display
- Mixer with channel strips and metering
- MIDI engine and piano roll editor
- VST3/AU plugin hosting
- Vim modal engine (Normal/Insert modes, hjkl, basic actions, visual cursor)
- VimStatusBar with mode/context/cursor/playhead display
- YAML session save/load

See `PRD.md` for full specification.

## Parallel Feature Branches

Managed via `wt` (dotfiles worktree tool). Bare repo at `~/git/drem-canvas.git/` with nested worktrees. Each worktree has its own build directory.

| Worktree | Branch | Scope |
|----------|--------|-------|
| `main/` | `master` | Integration branch |
| `feature/vim-commands/` | `feature/vim-commands` | Phase 5: operators, visual mode, command line, search, registers |
| `feature/advanced-editing/` | `feature/advanced-editing` | Phase 11: clip drag/trim/fades, crossfades, undo polish, tempo map |
| `feature/git-integration/` | `feature/git-integration` | Phase 12: git commands, semantic diff, bounce/export, automation |
| `feature/mixer-implementation/` | `feature/mixer-implementation` | Phase 6: vim mixer context, fader/pan modes, strip selection, master bus |

Create new feature: `wt new feature/my-feature`
Remove feature: `wt rm feature/my-feature`

Merge workflow (from `main/`):
```bash
git merge feature/vim-commands
git merge feature/mixer-implementation
git merge feature/advanced-editing
git merge feature/git-integration
```
