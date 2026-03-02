# Agent: Integration & Higher-Level Tests

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phases 7–8 of the test harness: write integration tests (session
round-trip, vim commands, audio graph) and higher-level model tests (Project, Track,
TempoMap, VimContext, ActionRegistry).

## Context

Read these specs before starting:

- `docs/test-harness/05-integration-and-advanced.md` (integration tests + higher-level model tests)
- `docs/test-harness/00-prd.md` (test directory layout)
- `src/model/Project.h` + `Project.cpp` (project model)
- `src/model/Track.h` + `Track.cpp` (track model)
- `src/model/Arrangement.h` + `Arrangement.cpp` (arrangement with track selection)
- `src/model/TempoMap.h` + `TempoMap.cpp` (tempo/time signature mapping)
- `src/model/GridSystem.h` + `GridSystem.cpp` (grid snapping)
- `src/model/Clipboard.h` + `Clipboard.cpp` (copy/paste)
- `src/vim/VimContext.h` + `VimContext.cpp` (panel focus, selection state)
- `src/vim/ActionRegistry.h` + `ActionRegistry.cpp` (fuzzy search, action lookup)
- `src/engine/TransportController.h` + `TransportController.cpp` (playback transport)
- `src/utils/SessionWriter.h` + `SessionWriter.cpp` (YAML serialisation)
- `src/utils/SessionReader.h` + `SessionReader.cpp` (YAML deserialisation)

Read the actual source files to understand APIs before writing tests.

## Prerequisites

Phases 1–6 must be completed. All dc:: unit tests must pass. The `dc_unit_tests`
and `dc_integration_tests` executables must exist.

## Deliverables

### Integration Tests (tests/integration/)

These link JUCE and use the custom `main.cpp` with `ScopedJuceInitialiser_GUI`.
Add to `target_sources(dc_integration_tests ...)`.

#### 1. tests/integration/test_session_roundtrip.cpp

Test session YAML serialisation/deserialisation:
- Create project with tracks, clips, mixer state → serialise → deserialise →
  compare all PropertyTree properties
- Empty project round-trip
- Project with MIDI clips (MidiSequence binary data)
- Project with audio clips (file paths preserved)
- Project with plugin state (base64-encoded binary)
- Mixer state: volume, pan, mute, solo
- Tempo changes round-trip

Use a temporary directory for serialisation output. Clean up after tests.

#### 2. tests/integration/test_vim_commands.cpp

Test Vim key sequences produce correct model state changes:
- `j` / `k` in arrangement: track selection moves
- `h` / `l` in arrangement: cursor moves
- `dd` on track: track deleted
- `yy` / `p`: track copied and pasted
- `u` after delete: track restored (undo)
- Mode transitions: Normal → Insert (`i`), Insert → Normal (`Escape`)
- Context switching: Tab cycles panels

Create a minimal project with tracks, feed key events through VimEngine, and
verify model state after each sequence.

#### 3. tests/integration/test_audio_graph.cpp

Test audio processing graph output:
- Silent graph: all outputs zero
- Single track with gain: output = input × gain
- Muted track: output zero
- Solo track: only solo track heard
- Plugin bypass: output equals input

Create a test audio graph, feed known audio buffers through it, and verify output
samples. Mock plugin instances if needed.

### Higher-Level Model Tests (tests/unit/model_layer/)

These are unit tests (no JUCE dependency). Add to `target_sources(dc_unit_tests ...)`.

#### 4. tests/unit/model_layer/test_project.cpp

- `addTrack()` / `removeTrack()`: track count changes
- `getTrack(index)` out of range: returns nullptr
- `getSampleRate()`: returns configured rate
- Undo after `addTrack()`: track removed

#### 5. tests/unit/model_layer/test_track.cpp

- `addClip()` at position: clip in track children
- `removeClip()`: clip removed
- Track properties (name, colour, volume, pan): set/get round-trip

#### 6. tests/unit/model_layer/test_arrangement.cpp

- Track selection: select, deselect, multi-select
- Clip selection: VimContext range

#### 7. tests/unit/model_layer/test_tempo_map.cpp

- `samplesToBeats()` at 120 BPM, 44100 SR: 44100 samples = 2 beats
- `beatsToSamples()` inverse: round-trip within ε
- `samplesToBarBeat()` with 4/4, 3/4, 6/8 time signatures
- Zero samples: beat 0, bar 1
- Very large sample counts: no overflow

#### 8. tests/unit/model_layer/test_grid_system.cpp

- Grid snap to beat: position quantised
- Grid snap to bar: position quantised
- Grid snap disabled: position unchanged

#### 9. tests/unit/model_layer/test_clipboard.cpp

- Copy clip → paste: new clip at cursor
- Copy track → paste: new track with same properties
- Cut removes source

### Vim Tests (tests/unit/vim/)

Add to `target_sources(dc_unit_tests ...)`.

#### 10. tests/unit/vim/test_vim_context.cpp

- Panel focus state transitions
- Visual selection track/clip range
- Grid cursor position (in samples)

#### 11. tests/unit/vim/test_action_registry.cpp

- `fuzzyScore()` correctness with known input/output pairs
- Action filtering by panel
- Action lookup by ID
- Empty query returns all actions

### Engine Tests (tests/unit/engine/)

Add to `target_sources(dc_unit_tests ...)`.

#### 12. tests/unit/engine/test_transport.cpp

- `play()` / `stop()` state: `isPlaying()` reflects
- `getPositionInSamples()`: atomic, correct value
- `setPosition()`: updates atomically
- Position after `stop()`: preserved (not reset)

## Important

- Integration tests link JUCE; unit tests do not. Keep them in separate executables.
- Session round-trip tests are the most valuable integration test — they verify that
  the entire PropertyTree ↔ YAML pipeline works end-to-end.
- Higher-level model tests may need to include headers from `src/model/` which may
  use `dc::PropertyTree` internally. Verify these compile without JUCE.
- If any model-layer class requires JUCE headers to compile (unlikely after migration),
  move that test to the integration test executable instead.
- Some higher-level classes (Project, Track, etc.) may not be fully decoupled from
  JUCE yet. If a class still depends on JUCE types, write only the tests that
  exercise dc:: APIs and note the JUCE dependency as a TODO.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to the appropriate `target_sources()` in `tests/CMakeLists.txt`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure`
