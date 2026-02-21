# Drem Canvas — feature/advanced-editing

## Mission

Implement **Phase 11** from `PRD.md`: Advanced editing features — clip manipulation, undo polish, tempo map, and editing tools.

## Build & Run

```bash
cmake --build build
open "build/DremCanvas_artefacts/Release/Drem Canvas.app"
```

If no `build/` dir: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`

## Architecture

- **C++17**, **JUCE 8**, namespace `dc`
- `src/model/` — ValueTree-based data model (Track, AudioClip, MidiClip, Arrangement)
- `src/gui/arrangement/` — ArrangementView, TrackLane, WaveformView (timeline UI)
- `src/utils/UndoSystem.h/.cpp` — UndoManager wrapper
- `src/model/TempoMap.h/.cpp` — Tempo/time-sig conversion utilities

## What to Implement

### 1. Clip Drag-and-Drop

In `TrackLane` / `ArrangementView`:
- Mouse drag to move clips along the timeline (update `startPosition` in ValueTree)
- Drag clips between tracks (remove from source track, add to target)
- Snap-to-grid based on tempo map grid divisions
- Visual feedback during drag (ghost clip at drop position)

### 2. Clip Trimming

- Drag left/right edges of clips to adjust `trimStart` / `trimEnd`
- Show resize cursor when hovering clip edges
- Respect original source file boundaries
- Waveform view updates in real-time during trim

### 3. Clip Fades

- Drag fade handles at clip corners to adjust `fadeInLength` / `fadeOutLength`
- Draw fade curves on WaveformView (linear or S-curve overlay)
- `AudioClip` model already has fade properties — wire them to `TrackProcessor`

### 4. Auto-Crossfades

- When clips overlap on the same track, automatically create a crossfade region
- Adjustable crossfade curve (linear, equal-power, S-curve)
- Visual crossfade indicator in TrackLane

### 5. Undo System Polish

Enhance `src/utils/UndoSystem.h/.cpp`:
- Transaction grouping — multiple related edits form one undoable action
- Coalescing for continuous changes (e.g., fader drags coalesce into one undo step)
- Every edit operation should be fully undoable via `Ctrl+Z` / `u` in vim
- Verify: make edit, undo, redo — state is identical

### 6. TempoMap Enhancements

Extend `src/model/TempoMap.h/.cpp`:
- Support tempo changes at arbitrary positions (tempo automation)
- Time-signature changes
- Bar/beat grid calculation for snap-to-grid
- Display grid lines in ArrangementView based on tempo

### 7. Selection System

- Multi-select clips (Shift+click or vim visual mode range)
- Rubber-band selection in arrangement view
- Selected clips highlighted, operations apply to all selected
- `selectAll()` / `deselectAll()` on Arrangement

### 8. Vim Editing Shortcuts

Wire these into `VimEngine` normal mode:
- `<` / `>` — nudge selected clip by grid unit
- `+` / `-` — zoom horizontally
- `Ctrl+` `+`/`-` — zoom vertically
- `J` — join adjacent regions on same track

## Key Files to Modify

- `src/gui/arrangement/TrackLane.h/.cpp` — Drag, trim, fade handles, crossfade visuals
- `src/gui/arrangement/WaveformView.h/.cpp` — Fade curve overlay
- `src/gui/arrangement/ArrangementView.h/.cpp` — Grid lines, rubber-band selection, zoom
- `src/model/AudioClip.h/.cpp` — Fade/crossfade model helpers
- `src/model/TempoMap.h/.cpp` — Tempo automation, grid calculations
- `src/model/Arrangement.h/.cpp` — Multi-selection
- `src/utils/UndoSystem.h/.cpp` — Transaction grouping, coalescing
- `src/engine/TrackProcessor.h/.cpp` — Apply fades during playback
- `src/vim/VimEngine.h/.cpp` — Nudge, zoom, join bindings

## Verification

- Drag a clip to a new position — undo restores it
- Trim a clip's start — waveform adjusts, playback respects trim
- Overlap two clips — crossfade appears and sounds correct
- Fader drag produces a single undo step (coalescing works)
- `<` / `>` nudges clip by one grid unit
- `+` / `-` zooms the arrangement view
- `J` joins two adjacent clips into one

## Conventions

- JUCE coding style (spaces around operators, camelCase methods, PascalCase classes)
- All new `.cpp` files go in `CMakeLists.txt` `target_sources`
- Always verify: `cmake --build build`
