# Drem Canvas — Digital Audio Workstation

## Context

Build a full-featured DAW as a native macOS desktop application using **C++ (C++17)** and the **JUCE 7+ framework**. JUCE provides the audio engine primitives (`AudioProcessorGraph`, `AudioDeviceManager`), GUI framework, plugin hosting (`VST3PluginFormat`), file I/O, and `ValueTree` data model — making it the industry-standard foundation for audio software.

Two differentiating features are designed in from the start:
1. **Vim-style modal navigation** — the entire DAW is operable from the keyboard with composable motions, operators, and modes
2. **Modular git-friendly session format** — project state is split into per-track/per-plugin YAML files for meaningful diffs and collaboration

## Architecture Overview

```
src/
├── Main.cpp / Application.h/cpp     # App entry point, JUCEApplication
├── engine/                           # Real-time audio code (never allocates/locks)
│   ├── AudioEngine.h/cpp            # Owns DeviceManager + ProcessorGraph
│   ├── AudioGraph.h/cpp             # Graph wrapper (add/remove/connect nodes)
│   ├── TransportController.h/cpp    # Play/stop/record/loop, atomic position
│   ├── TrackProcessor.h/cpp         # Per-track processor (clips + effects chain)
│   ├── MixBusProcessor.h/cpp        # Master bus: summing, gain, metering
│   ├── MetronomeProcessor.h/cpp     # Click track synced to tempo
│   └── MidiEngine.h/cpp             # MIDI device routing + recording
├── model/                            # Data model (message thread only)
│   ├── Project.h/cpp                # Root state, save/load coordinator
│   ├── Track.h/cpp                  # Track data (clips, name, volume, pan, etc.)
│   ├── AudioClip.h/cpp             # Audio region (source file, position, trim, fade)
│   ├── MidiClip.h/cpp              # MIDI region (MidiMessageSequence)
│   ├── Arrangement.h/cpp           # Track list management
│   ├── MixerState.h/cpp            # Mixer-specific view of track data
│   ├── TempoMap.h/cpp              # Tempo/time-signature automation
│   └── serialization/               # Modular YAML session format
│       ├── YAMLEmitter.h/cpp        # Internal state → YAML documents
│       ├── YAMLParser.h/cpp         # YAML documents → internal state
│       ├── SessionWriter.h/cpp      # Writes modular directory structure
│       └── SessionReader.h/cpp      # Reads modular directory structure
├── plugins/                          # Plugin hosting subsystem
│   ├── PluginManager.h/cpp          # VST3 scanning, KnownPluginList
│   ├── PluginHost.h/cpp             # Instantiate, state save/restore
│   └── PluginWindowManager.h/cpp    # Plugin editor window lifecycle
├── vim/                              # Vim modal navigation system
│   ├── VimEngine.h/cpp              # State machine, key parser, operator-pending logic
│   ├── VimCommandLine.h/cpp         # Command-mode widget, parser, completion, history
│   ├── VimRegisters.h/cpp           # Yank/paste registers, marks
│   └── VimContext.h/cpp             # Context-specific binding dispatch
├── gui/                              # All UI components
│   ├── MainWindow.h/cpp / MainComponent.h/cpp
│   ├── arrangement/                 # Timeline view
│   │   ├── ArrangementView, TrackLane, WaveformView
│   │   ├── MidiClipView, TimeRuler, Cursor
│   ├── mixer/                       # Mixer panel
│   │   ├── MixerPanel, ChannelStrip, MeterComponent
│   ├── transport/TransportBar.h/cpp
│   ├── midieditor/                  # Piano roll
│   │   ├── PianoRollEditor, PianoKeyboard, NoteComponent
│   ├── browser/BrowserPanel.h/cpp   # File + plugin browser
│   ├── vim/                         # Vim UI widgets
│   │   └── VimStatusBar.h/cpp       # Mode indicator, command line, cursor info
│   └── common/                      # LookAndFeel, Knob, Colours
└── utils/                            # Helpers
    ├── AudioFileUtils.h/cpp         # FormatManager, reader creation
    ├── ThreadPool.h/cpp             # Background waveform generation
    └── UndoSystem.h/cpp             # UndoManager wrapper with coalescing
```

**Key design principles:**
- **engine/** is real-time safe — no allocations, locks, or I/O on the audio thread
- **model/** is the single source of truth — GUI observes state changes, dispatches edits back to model
- **gui/** is purely presentational — never touches the audio engine directly
- **vim/** is the keyboard input layer — intercepts key events before any GUI widget processing
- Audio thread communicates with GUI via `std::atomic` values and lock-free FIFOs

## Build System

- CMake with JUCE as a git submodule
- `yaml-cpp` as a build dependency (for modular session format)
- `juce_add_gui_app` target linking: `juce_audio_basics`, `juce_audio_devices`, `juce_audio_formats`, `juce_audio_processors`, `juce_audio_utils`, `juce_core`, `juce_data_structures`, `juce_gui_basics`, `juce_gui_extra`
- Compile definitions: `JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1`

---

## Vim-Style Modal Navigation

### Modes

| Mode | Entry | Behavior |
|------|-------|----------|
| **Normal** (default) | `Escape` from any mode | Keys execute motions, selections, and actions — never type text |
| **Visual** | `v` from Normal | Extend selection across multiple items; highlighting follows cursor |
| **Visual-Line** | `V` from Normal | Select entire tracks (rows) |
| **Insert** | `i` when text field focused | All keys pass through to the widget — no vim interception |
| **Command** | `:` from Normal | Command-line prompt at bottom of window for typed commands |

### Normal Mode Navigation

- **hjkl** — move selection cursor (region left, track down, track up, region right)
- **Shift+hjkl** — move playhead / scroll viewport
- **Ctrl+hjkl** — jump by larger increments (previous marker, bottom/top of visible tracks, next marker)
- **0 / $** — jump to session start / end
- **gg / G** — first track / last track
- **w / b** — next / previous region boundary
- **f{char}** — jump to marker whose name starts with `{char}`
- **zz / zt / zb** — center / top / bottom current track in viewport

### Normal Mode Actions

| Key | Action |
|-----|--------|
| `x` | Delete selected region(s) |
| `d` | Begin delete operator (e.g., `dw` = delete to next region boundary) |
| `y` | Yank (copy) selected region(s) |
| `p` / `P` | Paste after / before playhead |
| `u` / `Ctrl+r` | Undo / redo |
| `s` | Split region at playhead |
| `m{char}` | Set named marker at playhead |
| `'{char}` | Jump to named marker |
| `.` | Repeat last action |
| `/` | Open search (regions, markers, tracks by name) |
| `n / N` | Next / previous search result |
| `Space` | Toggle play/stop |
| `r` | Toggle record-enable on selected track |
| `M` | Toggle mute |
| `S` | Toggle solo |
| `Enter` | Open focused item for editing (plugin GUI, enter piano roll) |
| `Tab` | Cycle focus between panels (Editor, Mixer, etc.) |

### Operator + Motion Composability

- `d` + motion — delete (e.g., `d$` = delete to end of track)
- `y` + motion — yank/copy
- `c` + motion — change (delete + enter insert mode)
- Number prefixes — `3j` = move down 3 tracks, `5x` = delete 5 regions

### Command Mode (`:`)

| Command | Action |
|---------|--------|
| `:w` | Save session |
| `:q` / `:wq` | Quit / save and quit |
| `:e <session>` | Open session |
| `:export` | Open export dialog |
| `:set tempo <bpm>` | Set tempo |
| `:set grid <div>` | Set grid division |
| `:track <name>` | Create new track |
| `:bus <name>` | Create new bus |
| `:plugin <name>` | Search and add plugin to selected track |
| `:mix` / `:edit` / `:rec` | Switch to mixer / editor / recorder view |
| `:<number>` | Jump to track number |

Tab-completion for commands, track names, and plugin names. Command history with up/down arrows.

### Context-Specific Bindings

**Arrangement context:**
- `s` splits at playhead, `J` joins adjacent regions
- `<` / `>` nudge region by grid unit
- `+` / `-` zoom horizontally, `Ctrl+` `+`/`-` zoom vertically

**Mixer context:**
- `h/l` select prev/next strip, `j/k` select prev/next plugin in chain
- `f` adjust fader (sub-mode: j/k for up/down), `A` add plugin

**Piano roll context:**
- `hjkl` move notes in time/pitch, `Shift+hjkl` by beat/octave
- `i` step-input mode, `v` velocity edit, `+`/`-` change note duration, `a` append note

### Registers and Marks

- `"ay` / `"ap` — yank/paste to/from register `a`
- `"+` — system clipboard
- `m{a-z}` / `'{a-z}` — set/jump to local marks
- `''` — jump to position before last jump

### Vim Status Bar

Persistent bar at the bottom of the main window:

```
+------------------------------------------------------------------+
| -- NORMAL --  | Editor | Track 3: "Drums" | Region: "kick_01"   |
+------------------------------------------------------------------+
```

Segments (left to right): **Mode** (color-coded), **Pending operator**, **Context**, **Cursor position**, **Playhead/grid info** (right-aligned).

In Command mode, the bar transforms into a text input with `:` prompt and tab-completion popup. In Search mode (`/`), shows incremental search with match count.

### Technical Integration (JUCE)

- `VimEngine` intercepts key events via JUCE's `KeyListener` on `MainComponent`, before any child component processes them
- In Insert mode, keys pass through to the focused JUCE widget
- In Normal/Visual/Command modes, `VimEngine` consumes the event and dispatches commands
- Commands map to actions on the model layer (same actions the GUI buttons trigger)
- `VimStatusBar` is a JUCE `Component` packed at the bottom of `MainComponent`'s layout

---

## Modular Git-Friendly Session Format

### Motivation

A single monolithic project file makes version control impractical — any change produces a diff of the entire file. The modular format splits state into small, focused YAML files so each change produces a minimal, human-readable diff.

### Session Directory Structure

```
MySession/
├── session.yaml                     # Metadata, config, tempo map, locations
├── sources.yaml                     # Audio/MIDI file references
├── routes/
│   ├── _manifest.yaml               # Route ordering, slug mapping, group memberships
│   ├── audio-1/
│   │   ├── route.yaml               # Route properties, IO, connections, panning
│   │   ├── playlist.yaml            # Regions with source references
│   │   ├── processors/
│   │   │   ├── _chain.yaml          # Processor ordering (signal flow)
│   │   │   ├── amp.yaml
│   │   │   ├── eq-1.yaml            # Plugin state + parameters
│   │   │   └── compressor-1.yaml
│   │   └── automation/
│   │       ├── gain.yaml            # Fader automation curve
│   │       ├── pan-azimuth.yaml
│   │       └── eq-1--frequency.yaml # Per-plugin-parameter automation
│   ├── drums-bus/
│   │   └── ...
│   └── master/
│       └── ...
├── mixer-scenes/
│   └── scene-1.yaml
├── interchange/
│   └── MySession/
│       ├── audiofiles/              # Audio recordings
│       └── midifiles/               # MIDI recordings
├── peaks/                           # Waveform cache (.gitignore)
├── export/                          # Export output (.gitignore)
└── .gitignore                       # Auto-generated
```

### File Format: YAML

YAML is chosen over XML because:
- **Cleanest diffs** — one parameter per line, no closing tags
- **Human-editable** — users can fix a broken session in a text editor
- **Comments** — annotatable by the application and by users
- **Merge-friendly** — git's line-based merge works naturally on flat key-value YAML

**C++ dependency:** `yaml-cpp` added as a build dependency.

### Key File Contents

**`session.yaml`** — session root:
```yaml
version: 1
name: MySession
sample-rate: 48000
tempo-map:
  tempos:
    - start: "1|1|0"
      bpm: 120.0
  meters:
    - start: "1|1|0"
      divisions-per-bar: 4.0
locations:
  - name: Verse
    start: 96000
    flags: [IsMark]
```

**`routes/<slug>/processors/<name>.yaml`** — individual plugin:
```yaml
id: 400
name: ACE EQ
active: true
unique-id: "urn:ardour:a-eq"
parameters:
  frequency: 1000
  gain: 3.5
  bandwidth: 1.0
```

**`routes/<slug>/automation/<name>.yaml`** — automation curve:
```yaml
parameter: gain
state: off
events:
  - [0, 1.0]
  - [48000, 0.5]
  - [96000, 1.0]
```

### Save Behavior

- **Full save:** All files written (on `:w`, Cmd+S, session close)
- **Incremental save:** Only dirty files rewritten (plugin tweak → rewrite that plugin's YAML only)
- **Atomic writes:** Each file written to `.tmp` first, then atomically renamed — crash-safe
- **Dirty tracking:** Per-file dirty flags keyed by path; `session.yaml` always rewritten to update ID counter

### Git Integration (Command Mode)

| Command | Action |
|---------|--------|
| `:git status` | Show modified session files |
| `:git diff` | Semantic summary of changes (e.g., "EQ on Drums: frequency 800 → 1200") |
| `:git commit <msg>` | Stage session files and commit |
| `:git log` | Show recent commits |
| `:git branch <name>` | Create branch (e.g., alternate mix) |
| `:git checkout <branch>` | Switch branches (reloads session) |

Uses `libgit2` or shells out to `git`. Operates only on session files — audio handled separately via Git LFS.

---

## Phased Implementation

### Phase 1: Skeleton App + Audio Passthrough
- Git init, JUCE submodule, `yaml-cpp` dependency, CMakeLists.txt
- `Main.cpp`, `Application`, `MainWindow`, `MainComponent`
- Open default CoreAudio device, pass input to output
- Audio device selector dialog
- **Verify:** App launches, mic input audible through speakers

### Phase 2: Audio Engine + Single-Track Playback
- `AudioEngine` (DeviceManager + ProcessorGraph + ProcessorPlayer)
- `TransportController` (play/stop, atomic position tracking)
- `TrackProcessor` with `AudioTransportSource` for disk streaming
- `TransportBar` GUI (play/stop buttons, time display, file open)
- **Verify:** Open WAV file, press play, hear audio, stop/seek works

### Phase 3: Multi-Track Arrangement + Clip Model
- `Project`, `Track`, `AudioClip` data model with save/load
- Multiple `TrackProcessor` nodes in graph with per-track volume/pan/mute/solo
- `ArrangementView`, `TrackLane`, `WaveformView` (using `AudioThumbnail`)
- `TimeRuler`, playback `Cursor`
- **Verify:** 3-4 audio tracks with clips at different positions, all mixed, waveforms visible

### Phase 4: Vim Modal Engine + Status Bar
- `VimEngine` state machine (Normal/Insert/Visual/Visual-Line/Command modes)
- Key interception via `KeyListener` on `MainComponent` — intercepts before child widgets
- `VimStatusBar` widget at bottom of window (mode display, context, cursor position)
- `hjkl` navigation in arrangement context (track/region selection)
- `Shift+hjkl` playhead/viewport movement
- Basic actions: `Space` (transport), `x` (delete), `u` (undo), `Ctrl+r` (redo)
- `Escape` returns to Normal from anywhere
- **Verify:** Navigate arrangement entirely by keyboard, status bar reflects mode and position

### Phase 5: Vim Operators, Visual Mode + Command Mode
- Operator-pending state (`d`, `y`, `c` waiting for motion)
- Motions: `w`, `b`, `0`, `$`, `gg`, `G`, number prefixes
- Visual mode with multi-region select, Visual-Line for whole-track selection
- Yank/paste with default register, `.` repeat
- `VimCommandLine` widget (`:` prompt, parser, tab-completion, history)
- Core commands: `:w`, `:q`, `:set tempo`, `:track`, `:bus`
- Search: `/`, `n`, `N` with incremental highlight
- Markers: `m{char}`, `'{char}`
- **Verify:** Compose `d3j` to delete 3 tracks, `:set tempo 140` changes tempo, `/kick` finds regions

### Phase 6: Mixer + Metering
- `MixBusProcessor` (master bus with summing and metering)
- `MixerPanel` with `ChannelStrip` per track (fader, pan knob, mute/solo)
- `MeterComponent` (stereo peak meter with hold, reads atomic floats at 30fps)
- Custom `LookAndFeel` (dark theme)
- Vim mixer context: `h/l` select strip, `j/k` select plugin, `f` fader sub-mode, `A` add plugin
- **Verify:** Faders/pan affect audio, meters animate, mixer navigable entirely by keyboard

### Phase 7: VST3 Plugin Hosting
- `PluginManager` (scan VST3 paths, `KnownPluginList` persisted)
- `PluginHost` (async instantiation, state save/restore)
- `PluginWindowManager` (editor windows)
- `TrackProcessor` updated with ordered plugin chain
- `BrowserPanel` with plugin list, drag-to-insert
- Vim: `:plugin <name>` to search and insert, `Enter` to open plugin GUI
- **Verify:** Scan finds plugins, insert reverb on track, hear effect, state persists across save/load

### Phase 8: Audio Recording
- `AudioRecorder` using `AudioFormatWriter::ThreadedWriter` (lock-free FIFO to disk)
- Track arm button, input routing during record
- Real-time waveform drawing during recording
- Punch-in/out with loop markers
- Vim: `r` toggles record-arm, `Space` starts/stops recording
- **Verify:** Arm track, record, new clip appears with waveform, plays back correctly

### Phase 9: MIDI Engine + Piano Roll
- `MidiEngine` (device enumeration, input routing, recording to `MidiMessageSequence`)
- `MidiClip` model, MIDI track variant of `TrackProcessor`
- `PianoRollEditor` (grid editor, draw/erase/select/resize tools, snap-to-grid)
- `PianoKeyboard`, `NoteComponent`, `MidiClipView` in arrangement
- Vim piano roll context: `hjkl` move notes, `Shift+hjkl` by beat/octave, `i` step-input, `v` velocity edit
- **Verify:** MIDI track + VSTi, play from keyboard, record, edit notes entirely by keyboard

### Phase 10: Modular Session Format
- `yaml-cpp` integration, `YAMLEmitter` / `YAMLParser` for each stateful class
- `SessionWriter` — writes modular directory structure (per-route, per-plugin, per-automation files)
- `SessionReader` — reads modular directory back into model
- Route slug generation and `_manifest.yaml`
- Processor chain splitting via `_chain.yaml`
- Automation curve files
- Auto-generated `.gitignore`
- Incremental save with per-file dirty tracking
- Atomic write safety (`.tmp` → rename)
- **Verify:** Save project, inspect YAML files, modify a plugin param, re-save — only that plugin's file changes

### Phase 11: Undo/Redo, Polish, Advanced Editing
- `UndoSystem` (transaction grouping, coalescing for continuous changes like fader drags)
- `TempoMap` (tempo/time-sig changes, bars-beats conversion)
- Clip drag-and-drop, trimming, fades, selection system, context menus
- Named vim registers (`"a`-`"z`, `"+` system clipboard)
- Macro recording (`q{char}` to record, `@{char}` to replay)
- User-configurable vim key mappings (`:map` / `:nmap` / `:vmap`)
- **Verify:** Every edit undoable, macros replay correctly, custom mappings work

### Phase 12: Git Integration + Advanced Features
- `:git` commands via `libgit2` or CLI (status, diff, commit, branch, checkout)
- Semantic diff display (parameter-level changes, not raw line diffs)
- Clip crossfades (auto-crossfade on overlap, adjustable curves)
- Per-parameter automation lanes (breakpoint editing, read/write/touch/latch modes)
- Bounce/export (offline render to WAV/AIFF/FLAC, stem export)
- Performance optimization (profile with Instruments.app, buffer pooling)
- **Verify:** `:git commit "rough mix"` works, `:git diff` shows readable parameter changes

### Phase 13: Stability + Distribution
- Unit tests (model serialization round-trips, TempoMap math, engine render tests, YAML parse/emit)
- Plugin crash handling, device disconnect recovery, missing file resolution
- macOS code signing, notarization, DMG installer, hardened runtime
- User documentation

---

## Key Technical Risks

| Risk | Mitigation |
|------|-----------|
| Audio thread safety violations | Strict convention: nothing in `engine/` allocates or locks. Use debug-mode guards. |
| Plugin compatibility | Scan out-of-process, sanitize NaN/Inf output, save state before risky operations |
| Model thread safety | Model lives on message thread only; audio thread gets atomic snapshots |
| Performance with many tracks/plugins | Use `AudioProcessorGraph` parallel processing, profile early, add track freeze |
| Vim key conflicts with OS/JUCE | `VimEngine` intercepts at `KeyListener` level before JUCE widget handling; Insert mode passes through |
| YAML serialization fidelity | Round-trip tests: save → load → save must produce identical YAML |
| Git merge conflicts in session | Modular format minimizes conflicts; provide graphical merge tool for remaining cases |

## Verification Strategy

Each phase has a concrete "verify" step. General testing approach:
1. Build with `cmake --build build` after each phase
2. Manual testing per phase verification criteria
3. Unit tests added in Phase 13 (but model + YAML round-trip tests can start in Phase 3/10)
4. Profiling with Instruments.app starting in Phase 6
