# Drem Canvas — feature/vim-commands

## Mission

Implement **Phase 5** from `PRD.md`: Vim Operators, Visual Mode, and Command Mode.

## Build & Run

```bash
cmake --build build
open "build/DremCanvas_artefacts/Release/Drem Canvas.app"
```

If no `build/` dir: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`

## Architecture

- **C++17**, **JUCE 8**, namespace `dc`
- `src/vim/VimEngine.h/.cpp` — State machine, key dispatch (currently Normal/Insert only)
- `src/vim/VimContext.h/.cpp` — Panel focus, clip selection, clipboard
- `src/gui/vim/VimStatusBar.h/.cpp` — Mode indicator at bottom of window
- `src/gui/MainComponent.h/.cpp` — VimEngine attached as KeyListener

## Key Patterns

- `ValueTree` is the single source of truth for all model state
- `VimEngine` is a `juce::KeyListener` attached to `MainComponent` — intercepts all keys in Normal mode, passes through in Insert mode
- Track selection lives in `Arrangement`, clip selection in `VimContext`
- `Project::getUndoManager()` provides the shared `juce::UndoManager`

## Playhead & Timeline Coordinate System

All timeline positions follow one coordinate chain. See `main` branch CLAUDE.md for full details.

```
SAMPLES ÷ sampleRate → SECONDS × pixelsPerSecond → TIMELINE PIXELS + headerWidth(150) - scrollOffset → SCREEN PIXELS
```

## What to Implement

### 1. Operator-Pending Mode

Extend `VimEngine` with an operator-pending state:
- `d` + motion = delete (e.g., `d$` delete to end of track, `d3j` delete 3 tracks)
- `y` + motion = yank/copy
- `c` + motion = change (delete + enter insert mode)
- Number prefixes: `3j` = move down 3 tracks, `5x` = delete 5 regions
- Pending operator shown in VimStatusBar

### 2. Visual Mode

Add `Visual` and `VisualLine` modes to `VimEngine::Mode`:
- `v` from Normal enters Visual — extends selection across regions
- `V` from Normal enters Visual-Line — selects entire tracks
- `hjkl` extends the selection range
- Operators (`d`, `y`, `c`) act on the visual selection then return to Normal
- `Escape` cancels and returns to Normal
- Visual selection must be rendered in `TrackLane` (highlight selected range)

### 3. VimCommandLine (`src/vim/VimCommandLine.h/.cpp`)

New component replacing the status bar when `:` is pressed:
- Text input with `:` prompt
- Tab-completion for commands, track names
- Command history (up/down arrows)
- `Enter` executes, `Escape` cancels
- Core commands: `:w` (save), `:q` (quit), `:wq`, `:set tempo <bpm>`, `:track <name>`, `:bus <name>`, `:<number>` (jump to track)

### 4. Search (`/`)

- `/` opens search prompt in status bar area
- Incremental search across regions, markers, track names
- `n` / `N` for next/prev match
- Match count shown in status bar

### 5. Additional Motions

- `w` / `b` — next/previous region boundary
- `f{char}` — jump to marker starting with char
- `m{char}` — set named marker at playhead
- `'{char}` — jump to named marker
- `.` — repeat last action

### 6. Registers and Marks

- Default register for yank/paste (already exists as clipboard in VimContext)
- Named registers `"a`-`"z` for multiple clipboards
- `"+` for system clipboard
- Marks stored in VimContext

## Key Files to Modify

- `src/vim/VimEngine.h/.cpp` — Add modes, operator-pending state, number prefix accumulator
- `src/vim/VimContext.h/.cpp` — Add registers, marks storage
- `src/gui/vim/VimStatusBar.h/.cpp` — Show pending operator, visual mode indicator
- `src/gui/arrangement/TrackLane.h/.cpp` — Visual mode selection rendering

## Key Files to Create

- `src/vim/VimCommandLine.h/.cpp` — Command-line widget and parser

## Verification

- `d3j` deletes 3 tracks worth of clips
- `v` + `jjl` + `d` selects and deletes a range
- `V` + `jj` + `y` yanks entire tracks
- `:set tempo 140` changes tempo
- `/kick` finds regions containing "kick"
- `.` repeats the last delete/yank/paste
- `"ay` yanks to register `a`, `"ap` pastes from it

## Conventions

- JUCE coding style (spaces around operators, camelCase methods, PascalCase classes)
- All new `.cpp` files go in `CMakeLists.txt` `target_sources`
- Always verify: `cmake --build build`
