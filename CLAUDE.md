# Drem Canvas — feature/git-integration

## Mission

Implement **Phase 12** from `PRD.md`: Git integration via vim command mode, bounce/export, and per-parameter automation.

## Build & Run

```bash
cmake --build build
open "build/DremCanvas_artefacts/Release/Drem Canvas.app"
```

If no `build/` dir: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`

## Architecture

- **C++17**, **JUCE 8**, namespace `dc`
- `src/model/` — ValueTree data model, YAML session format in `serialization/`
- `src/engine/BounceProcessor.h/.cpp` — Offline render (exists, needs enhancement)
- `src/vim/VimEngine.h/.cpp` — Vim key handling (Normal/Insert modes)
- Session files are YAML in a modular directory structure (see PRD.md "Modular Git-Friendly Session Format")

## What to Implement

### 1. Git Commands (via shell)

Create `src/utils/GitIntegration.h/.cpp`:
- Shell out to `git` CLI (not libgit2 — simpler, `git` is always available on macOS)
- Run commands asynchronously on a background thread, post results to message thread
- Operations scoped to the session directory (`currentSessionDirectory` in MainComponent)

Commands to support (triggered from vim command mode, which is Phase 5 — for now, expose as callable functions that can be wired later):

| Function | Git command | Description |
|----------|------------|-------------|
| `gitStatus()` | `git status --short` | Show modified session files |
| `gitDiff()` | `git diff` | Show raw diff of session files |
| `gitCommit(msg)` | `git add -A && git commit -m "msg"` | Stage and commit session files |
| `gitLog(n)` | `git log --oneline -n` | Recent commits |
| `gitBranch(name)` | `git checkout -b name` | Create branch (alternate mix) |
| `gitCheckout(branch)` | `git checkout branch` | Switch branch (triggers session reload) |

### 2. Semantic Diff Display

Create `src/utils/SemanticDiff.h/.cpp`:
- Parse YAML session files (before/after) and produce human-readable change descriptions
- Example: instead of raw YAML diff, show "EQ on Drums: frequency 800 -> 1200"
- Compare `session.yaml`, `route.yaml`, plugin YAML files
- Output as a list of `ChangeDescription { trackName, pluginName, paramName, oldValue, newValue }`

### 3. Bounce/Export

Enhance `src/engine/BounceProcessor.h/.cpp`:
- Offline render of the full mix to an audio file
- Support WAV, AIFF, FLAC output formats
- Stem export: render each track individually
- Progress callback for UI feedback
- Bit depth options: 16, 24, 32-float

Create export dialog or wire to a future `:export` command:
- Output path selection
- Format selection
- Sample rate selection
- Normalize option

### 4. Per-Parameter Automation Lanes

Extend the model for automation:
- `src/model/AutomationLane.h/.cpp` — Automation curve (list of time-value breakpoints)
- Each track can have multiple automation lanes (volume, pan, plugin parameters)
- Automation modes: Off, Read, Write, Touch, Latch
- Store as `automation/<param-name>.yaml` in session directory

Create automation UI:
- `src/gui/arrangement/AutomationLane.h/.cpp` already exists — extend it with:
  - Breakpoint editing (click to add, drag to move, delete key to remove)
  - Curve display overlaid on track lane
  - Mode selector (Off/Read/Write/Touch/Latch)

Wire automation to audio engine:
- `TrackProcessor` reads automation values during playback
- Write mode records parameter changes as automation

### 5. Auto-Generated .gitignore

When saving a session, write a `.gitignore` file to the session directory:
```
peaks/
export/
*.tmp
```

This should be added to `SessionWriter`.

## Key Files to Create

- `src/utils/GitIntegration.h/.cpp` — Git CLI wrapper
- `src/utils/SemanticDiff.h/.cpp` — YAML diff to human-readable changes
- `src/model/AutomationLane.h/.cpp` — Automation curve model

## Key Files to Modify

- `src/engine/BounceProcessor.h/.cpp` — Full offline render, stem export, format options
- `src/gui/arrangement/AutomationLane.h/.cpp` — Breakpoint editing UI
- `src/model/serialization/SessionWriter.h/.cpp` — Write .gitignore, automation files
- `src/model/serialization/SessionReader.h/.cpp` — Read automation files
- `CMakeLists.txt` — Add new source files

## Verification

- `gitStatus()` returns list of modified YAML files after a parameter change
- `gitCommit("rough mix")` creates a git commit in the session directory
- `gitLog(5)` shows recent commits
- `gitCheckout("branch")` switches branch and reloads session
- Semantic diff shows "Track 1 volume: 0.8 -> 0.6" instead of raw YAML
- Bounce produces a valid WAV file of the full mix
- Stem export produces one file per track
- Automation lane records and plays back volume changes
- `.gitignore` is auto-created on session save

## Conventions

- JUCE coding style (spaces around operators, camelCase methods, PascalCase classes)
- All new `.cpp` files go in `CMakeLists.txt` `target_sources`
- Always verify: `cmake --build build`
- Run git commands via `juce::ChildProcess` for async execution
