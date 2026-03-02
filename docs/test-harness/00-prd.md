# Test Harness — Master PRD

> Add testing infrastructure to Drem Canvas: framework integration, migration
> correctness tests, architecture enforcement, agent guardrails, and CI/CD.

**Branch**: `feature/test-harness`
**Dependencies**: Phases 0–2 of sans-JUCE migration (completed)
**Related**: [01-cmake-infrastructure.md](01-cmake-infrastructure.md),
[02-migration-tests.md](02-migration-tests.md),
[03-architecture-guards.md](03-architecture-guards.md),
[04-agent-management.md](04-agent-management.md),
[05-integration-and-advanced.md](05-integration-and-advanced.md)

---

## Motivation

Drem Canvas has ~37,900 LOC across 296 files and zero test coverage. The sans-JUCE
migration (Phases 0–2) replaced JUCE foundation types, ValueTree, MIDI types, and
audio I/O with `dc::` equivalents — four new libraries totalling ~2,800 LOC of
reimplemented infrastructure. None of this code has automated tests.

Without tests:
- **Migration bugs hide** — subtle differences between `juce::ValueTree` and
  `dc::PropertyTree` go unnoticed until they cause runtime failures
- **Regressions accumulate** — each feature branch can silently break dc:: invariants
- **AI agents operate blind** — Claude Code sessions modify code with no automated
  feedback on correctness
- **Refactoring is risky** — future phases (plugin hosting, GUI migration) cannot
  safely change dc:: internals

The test harness addresses all four problems in a phased rollout that starts with
CMake infrastructure and ends with CI/CD integration.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        CI / CD                              │
│  GitHub Actions: build → test → coverage → fuzz (nightly)   │
├─────────────────────────────────────────────────────────────┤
│                   Agent Guardrails                          │
│  hooks (PostToolUse/Stop) → quick-check.sh / verify.sh      │
├─────────────────────────────────────────────────────────────┤
│                Architecture Enforcement                     │
│  check_architecture.sh │ #error guards │ CMake link-time    │
├─────────────────────────────────────────────────────────────┤
│                    Test Suites                               │
│                                                             │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐ │
│  │  Unit Tests   │ │ Integration  │ │  Fuzz / Property     │ │
│  │  dc_unit_tests│ │ dc_integ_    │ │  fuzz_midi_parser    │ │
│  │              │ │ tests        │ │  fuzz_yaml_session   │ │
│  └──────┬───────┘ └──────┬───────┘ └──────────┬───────────┘ │
│         │                │                     │             │
├─────────┴────────────────┴─────────────────────┴─────────────┤
│                  CMake Test Infrastructure                    │
│  Catch2 v3 │ Trompeloeil │ CTest │ test/coverage presets     │
├─────────────────────────────────────────────────────────────┤
│               dc:: Library Targets                           │
│                                                             │
│  ┌──────────────┐ ┌──────────┐ ┌────────┐ ┌──────────────┐ │
│  │ dc_foundation │ │ dc_model │ │dc_midi │ │  dc_audio    │ │
│  │ (pure C++17)  │ │          │ │        │ │ +sndfile     │ │
│  │ no deps       │ │ +found.  │ │+found. │ │ +portaudio   │ │
│  └──────────────┘ └──────────┘ └────────┘ └──────────────┘ │
│                 NO JUCE LINKAGE                              │
└─────────────────────────────────────────────────────────────┘
```

### Framework Choices

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Test framework | **Catch2 v3.8.1** | JUCE ecosystem standard; SECTION blocks; string literal test names; built-in benchmarking; first-class CMake integration |
| Mocking | **Trompeloeil** | Header-only; dedicated Catch2 adapter; RAII expectations; no vtable hacking |
| Property-based | **RapidCheck** | QuickCheck for C++; Catch2 integration via extras/catch; automatic shrinking |
| Fuzzing | **libFuzzer** | Ships with Clang (already required for Skia); dictionary support; corpus seeding |
| Coverage | **llvm-cov** | Source-based (expression-level); Clang-native; exports to lcov format for CI |
| Mutation testing | **Mull** | LLVM IR-level; framework-agnostic; runs on any test binary |
| Snapshots | **ApprovalTests.cpp** | Golden master testing; Catch2-compatible; `.approved`/`.received` workflow |

---

## Phase Structure

| Phase | Name | Deliverables | What It Proves |
|-------|------|-------------|----------------|
| **1** | CMake infrastructure | `dc_foundation`, `dc_model`, `dc_midi`, `dc_audio` library targets; Catch2 FetchContent; `tests/` directory; `test` and `coverage` CMake presets; CTest integration | dc:: libraries build independently without JUCE |
| **2** | Architecture guards | `scripts/check_architecture.sh`; compile-time `#error` guards in dc:: headers; `.clang-tidy` configuration | Migration boundaries are enforced at script, compile, and link time |
| **3** | Foundation tests | `test_colour.cpp`, `test_string_utils.cpp`, `test_base64.cpp`, `test_random.cpp`, `test_spsc_queue.cpp`, `test_message_queue.cpp`, `test_listener_list.cpp`, `test_worker_thread.cpp` | Phase 0 replacements are correct |
| **4** | Model tests | `test_variant.cpp`, `test_property_id.cpp`, `test_property_tree.cpp`, `test_undo_manager.cpp` | Phase 1 replacements are correct |
| **5** | MIDI tests | `test_midi_message.cpp`, `test_midi_buffer.cpp`, `test_midi_sequence.cpp` | Phase 2 MIDI replacements are correct |
| **6** | Audio tests | `test_audio_block.cpp`, `test_audio_file_io.cpp`, `test_disk_streamer.cpp`, `test_threaded_recorder.cpp` | Phase 2 audio replacements are correct |
| **7** | Integration tests | `test_session_roundtrip.cpp`, `test_vim_commands.cpp`, `test_audio_graph.cpp`; custom `main.cpp` with JUCE initialiser | End-to-end model layer and serialisation work |
| **8** | Higher-level model tests | `test_project.cpp`, `test_track.cpp`, `test_tempo_map.cpp`, `test_grid_system.cpp`, `test_vim_context.cpp`, `test_action_registry.cpp` | Application logic on top of dc:: model is correct |
| **9** | Agent guardrails | `scripts/quick-check.sh`, `scripts/verify.sh`; Claude Code hooks configuration; CLAUDE.md verification section | AI agent sessions get automated feedback |
| **10** | Fuzz, coverage, CI | libFuzzer harnesses; llvm-cov coverage preset; GitHub Actions workflow; mutation testing (Mull) | Hardening and continuous integration |

### Phase Dependencies

```
Phase 1 ──→ Phase 2 ──→ Phase 9 (guardrails need scripts from Phase 2)
   │
   ├──→ Phase 3 ──→ Phase 4 ──→ Phase 5 ──→ Phase 6
   │                                            │
   │                                            ├──→ Phase 7 (integration)
   │                                            └──→ Phase 8 (higher-level)
   │
   └──→ Phase 10 (coverage/fuzz need test targets from Phase 1)
```

Phases 3–6 can run in parallel once Phase 1 is complete. Phase 7 requires
Phases 5–6. Phase 10 can start any time after Phase 1.

---

## Test Directory Layout

```
tests/
├── CMakeLists.txt                  # Test targets and Catch2 integration
├── unit/
│   ├── foundation/
│   │   ├── test_colour.cpp
│   │   ├── test_string_utils.cpp
│   │   ├── test_base64.cpp
│   │   ├── test_random.cpp
│   │   ├── test_spsc_queue.cpp
│   │   ├── test_message_queue.cpp
│   │   ├── test_listener_list.cpp
│   │   └── test_worker_thread.cpp
│   ├── model/
│   │   ├── test_variant.cpp
│   │   ├── test_property_id.cpp
│   │   ├── test_property_tree.cpp
│   │   └── test_undo_manager.cpp
│   ├── midi/
│   │   ├── test_midi_message.cpp
│   │   ├── test_midi_buffer.cpp
│   │   └── test_midi_sequence.cpp
│   ├── audio/
│   │   ├── test_audio_block.cpp
│   │   ├── test_audio_file_io.cpp
│   │   ├── test_disk_streamer.cpp
│   │   └── test_threaded_recorder.cpp
│   ├── model_layer/
│   │   ├── test_project.cpp
│   │   ├── test_track.cpp
│   │   ├── test_arrangement.cpp
│   │   ├── test_tempo_map.cpp
│   │   ├── test_grid_system.cpp
│   │   └── test_clipboard.cpp
│   ├── vim/
│   │   ├── test_vim_context.cpp
│   │   └── test_action_registry.cpp
│   └── engine/
│       └── test_transport.cpp
├── integration/
│   ├── main.cpp                    # Custom main with ScopedJuceInitialiser_GUI
│   ├── test_session_roundtrip.cpp
│   ├── test_vim_commands.cpp
│   └── test_audio_graph.cpp
├── regression/
│   └── .gitkeep                    # Bug-fix tests accumulate here
├── fuzz/
│   ├── fuzz_midi_parser.cpp
│   ├── fuzz_yaml_session.cpp
│   └── fuzz_vim_keys.cpp
├── golden/
│   └── basic-session.approved.yaml
└── fixtures/
    ├── test_mono_44100.wav
    └── test_stereo_48000.wav
```

---

## Test Categories

| Category | Executable | Links JUCE? | CI Frequency | Hardware? |
|----------|-----------|-------------|-------------|-----------|
| Unit tests | `dc_unit_tests` | No | Every push | No |
| Integration tests | `dc_integration_tests` | Yes | Every push | xvfb (Linux) |
| Fuzz tests | `fuzz_*` | No | Nightly (60s/target) | No |
| GPU rendering | manual | Yes | Manual trigger | GPU runner |
| Plugin hosting | manual | Yes | Weekly | Test VST3 binaries |

---

## Verification Criteria

Each phase has a concrete exit criterion:

1. **CMake infrastructure**: `cmake --preset test && cmake --build --preset test` succeeds;
   `ctest --test-dir build-debug` runs 0 tests (no test files yet), exits 0
2. **Architecture guards**: `scripts/check_architecture.sh` exits 0; `#error` triggers
   on intentional JUCE include in dc:: source
3. **Foundation tests**: All foundation tests pass; mutation score ≥ 80% on foundation
4. **Model tests**: All model tests pass; PropertyTree undo/redo round-trips verified
5. **MIDI tests**: All MIDI tests pass; legacy JUCE format deserialization verified
6. **Audio tests**: AudioFileReader/Writer round-trip across WAV/FLAC/AIFF formats
7. **Integration tests**: Session YAML round-trip preserves all properties
8. **Higher-level tests**: TempoMap coordinate transforms round-trip within ε
9. **Agent guardrails**: `scripts/verify.sh` exits 0; hooks fire on Edit/Write
10. **Fuzz/CI**: GitHub Actions workflow passes on ubuntu-latest and macos-latest

---

## Priority Order

Start with Phases 1–2 (infrastructure + guards) — these provide immediate value by
preventing migration regressions. Then Phases 3–6 (proving migration correctness).
Phases 7–8 extend coverage to application logic. Phase 9 adds agent guardrails.
Phase 10 adds hardening.

The CMake library separation in Phase 1 is itself the strongest migration guard:
if `dc_foundation` does not link JUCE, any `#include <JuceHeader.h>` in a foundation
source file causes a compile or link error.
