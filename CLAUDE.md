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

Run:

```bash
open "build/DremCanvas_artefacts/Release/Drem Canvas.app"
```

## Architecture

- **C++17** with **JUCE 8** framework, **yaml-cpp** for serialization
- Namespace: `dc`
- `src/engine/` — Real-time audio (never allocates or locks on audio thread)
- `src/model/` — Data model using JUCE `ValueTree` (message thread only)
- `src/gui/` — Presentational components observing model state
- `src/vim/` — Vim modal navigation (KeyListener intercepting before child widgets)
- `src/plugins/` — VST3/AU plugin hosting
- `src/utils/` — Helpers (undo, audio file utils)

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
6. App launches without crash: `open "build/DremCanvas_artefacts/Release/Drem Canvas.app"`

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
