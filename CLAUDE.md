# Drem Canvas — DAW Project

## Build

```bash
cmake --build build
```

If the build directory doesn't exist:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
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

## Conventions

- JUCE coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use `<JuceHeader.h>` plus project-relative paths (e.g., `"model/Project.h"`)
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Always verify with `cmake --build build` after changes

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

Three features are being developed in parallel via git worktrees. When merging, integrate one at a time and resolve conflicts before the next.

| Worktree | Branch | Scope |
|----------|--------|-------|
| `../drem-canvas-vim-commands/` | `feature/vim-commands` | Phase 5: operators, visual mode, command line, search, registers |
| `../drem-canvas-advanced-editing/` | `feature/advanced-editing` | Phase 11: clip drag/trim/fades, crossfades, undo polish, tempo map |
| `../drem-canvas-git-integration/` | `feature/git-integration` | Phase 12: git commands, semantic diff, bounce/export, automation |

Merge workflow:
```bash
git merge feature/vim-commands
git merge feature/advanced-editing
git merge feature/git-integration
```
