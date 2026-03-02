# Sans-JUCE Migration — Master PRD

> Remove JUCE entirely from Drem Canvas. Replace with custom implementations
> and targeted third-party libraries. Codename: **Option C**.

## Motivation

JUCE provides a comprehensive framework, but bundling it creates friction:

1. **Licensing** — JUCE's GPL/commercial license constrains distribution
2. **Binary size** — Linking 11 JUCE modules inflates the binary with unused GUI code
3. **Patch burden** — Local JUCE patches (IParameterFinder, performEdit snoop) must be
   rebased on every upstream update
4. **Abstraction mismatch** — Graphics, windowing, event dispatch, serialization, and
   dialogs have already been decoupled; JUCE remains only for audio plumbing, plugin
   hosting, and foundation types
5. **Build complexity** — JUCE's CMake integration adds configuration weight

## Scope

**In scope** — Remove every `juce::` symbol and `#include <JuceHeader.h>` from the
codebase. Replace with:

| Subsystem | Replacement |
|-----------|------------|
| Data model (`ValueTree`, `Identifier`, `var`) | `dc::PropertyTree` ([01-observable-model.md](01-observable-model.md)) |
| Audio graph (`AudioProcessorGraph`) | `dc::AudioGraph` ([02-audio-graph.md](02-audio-graph.md)) |
| Plugin hosting (`AudioPluginFormatManager`) | Steinberg VST3 SDK direct ([03-plugin-hosting.md](03-plugin-hosting.md)) |
| Foundation types (`String`, `File`, `Array`) | `std::` equivalents + thin `dc::` wrappers ([04-foundation-types.md](04-foundation-types.md)) |
| Audio device I/O (`AudioDeviceManager`) | PortAudio ([05-audio-io.md](05-audio-io.md)) |
| Audio file I/O (`AudioFormatManager`) | libsndfile ([05-audio-io.md](05-audio-io.md)) |
| MIDI types & I/O (`MidiMessage`, `MidiBuffer`) | Custom types + RtMidi ([06-midi-subsystem.md](06-midi-subsystem.md)) |
| Undo system (`UndoManager`) | `dc::UndoManager` ([07-undo-system.md](07-undo-system.md)) |

**Out of scope** — Graphics (already Skia), windowing (already Cocoa/GLFW), event
dispatch (already custom), serialization (already yaml-cpp), file dialogs (already
native/zenity).

## Target Architecture

```
┌──────────────────────────────────────────────────────┐
│                   Application Layer                   │
│   Main.cpp · AppController · VimEngine · UI Widgets   │
├──────────────┬───────────────┬────────────────────────┤
│  dc::model   │  dc::engine   │     dc::plugins        │
│ PropertyTree │  AudioGraph   │  VST3Host (SDK direct) │
│ Variant/Id   │  AudioNode    │  PluginInstance         │
│ UndoManager  │  BufferPool   │  PluginScanner          │
│              │  GraphExecutor│  PluginEditor           │
├──────────────┼───────────────┼────────────────────────┤
│           dc::audio          │      dc::midi           │
│  AudioDeviceManager (PA)     │  MidiMessage/Buffer     │
│  AudioFileReader (sndfile)   │  MidiDeviceManager (Rt) │
│  DiskStreamer · Recorder     │  MidiSequence           │
├──────────────────────────────┴────────────────────────┤
│                  dc::foundation                        │
│  MessageQueue · ListenerList · SPSCQueue · config.h    │
├───────────────────────────────────────────────────────┤
│                  Platform Layer                        │
│  Cocoa/Metal (macOS) · GLFW/Vulkan (Linux) · X11      │
├───────────────────────────────────────────────────────┤
│                  Third-Party Libraries                 │
│  Skia · PortAudio · libsndfile · RtMidi · VST3 SDK   │
│  yaml-cpp · (future: CoreAudio/ALSA native)           │
└───────────────────────────────────────────────────────┘
```

## Phasing

Five phases plus a foundation sweep. Each phase produces a compilable, runnable
binary. Phases 1-2 can execute in parallel on separate worktrees.

### Phase 0 — Foundation Sweep

**Goal**: Replace all JUCE foundation types (`String`, `File`, `Array`, `Colour`,
`Random`, `Time`, `CriticalSection`, macros) with `std::` or `dc::` equivalents.
This unblocks every other phase.

**Spec**: [04-foundation-types.md](04-foundation-types.md)

**Deliverables**:
- `dc/foundation/` header library
- CMake-generated `config.h` (app name, version)
- All `juce::String` → `std::string`, `juce::File` → `std::filesystem::path`, etc.
- Macro replacements: `jassert` → `dc_assert`, `DBG` → `dc_log`, etc.
- `dc::MessageQueue` replacing `MessageManager::callAsync()`
- `dc::ListenerList<T>` replacing `juce::ListenerList`

**Verification**: `grep -r "juce::String\|juce::File\|juce::Array\|juce::Colour" src/`
returns zero hits (excluding engine/, plugins/, model/ ValueTree code).

---

### Phase 1 — Observable Model + Undo

**Goal**: Replace `juce::ValueTree` / `juce::Identifier` / `juce::var` /
`juce::UndoManager` with custom implementations.

**Specs**: [01-observable-model.md](01-observable-model.md), [07-undo-system.md](07-undo-system.md)

**Deliverables**:
- `dc::PropertyTree` — observable hierarchical model
- `dc::PropertyId` — string-interned identifiers
- `dc::Variant` — typed union (`int64_t`, `double`, `bool`, `std::string`, `BinaryBlob`)
- `dc::PropertyTree::Listener` — 5 callbacks matching existing ValueTree::Listener
- `dc::UndoManager` — action stack with transactions and coalescing
- YAML serializer updated for PropertyTree
- All model/ files migrated

**Dependencies**: Phase 0 (foundation types)

**Verification**: Session save/load round-trip produces identical YAML. Undo/redo
works for all model mutations.

---

### Phase 2 — Audio I/O + MIDI

**Goal**: Replace JUCE audio device management, audio file I/O, and MIDI subsystem.

**Specs**: [05-audio-io.md](05-audio-io.md), [06-midi-subsystem.md](06-midi-subsystem.md)

**Deliverables**:
- `dc::AudioDeviceManager` — PortAudio wrapper behind abstract interface
- `dc::AudioFileReader` / `dc::AudioFileWriter` — libsndfile wrappers
- `dc::DiskStreamer` — background disk reader with ring buffer
- `dc::ThreadedRecorder` — lock-free recording to disk
- `dc::MidiMessage` / `dc::MidiBuffer` / `dc::MidiSequence`
- `dc::MidiDeviceManager` — RtMidi wrapper
- `dc::SPSCQueue<T>` — lock-free single-producer single-consumer ring buffer
- Waveform cache migrated to use `dc::AudioFileReader`

**Dependencies**: Phase 0 (foundation types)

**Verification**: Audio playback works. Recording produces valid files. MIDI input
routes correctly. Waveforms display.

---

### Phase 3 — Audio Graph

**Goal**: Replace `juce::AudioProcessorGraph` with a custom parallel DAG engine.

**Spec**: [02-audio-graph.md](02-audio-graph.md)

**Deliverables**:
- `dc::AudioNode` — minimal processor interface
- `dc::AudioGraph` — topology management with incremental topological sort
- `dc::BufferPool` — pre-allocated buffer recycling (zero alloc on audio thread)
- `dc::GraphExecutor` — work-stealing thread pool for parallel branch execution
- `dc::AudioBlock` / `dc::MidiBlock` — non-owning buffer views
- Plugin Delay Compensation (PDC)
- All existing processors (TrackProcessor, MixBusProcessor, etc.) migrated to AudioNode

**Dependencies**: Phase 2 (audio I/O types for AudioBlock/MidiBlock)

**Verification**: Multi-track playback with plugins processes correctly. Latency
compensation is accurate. CPU usage comparable or better than JUCE graph.

---

### Phase 4 — VST3 Plugin Hosting

**Goal**: Replace JUCE plugin hosting with direct Steinberg VST3 SDK integration.

**Spec**: [03-plugin-hosting.md](03-plugin-hosting.md)

**Deliverables**:
- `dc::VST3Host` — module loading, factory enumeration, component creation
- `dc::PluginInstance` — wraps IComponent/IAudioProcessor, implements AudioNode
- `dc::PluginDescription` — metadata struct
- `dc::PluginScanner` — out-of-process scanning with crash isolation
- `dc::PluginEditor` — IPlugView lifecycle, platform window embedding
- Native IParameterFinder (eliminates JUCE patches entirely)
- Native performEdit snoop via IComponentHandler
- State save/restore (binary blobs, base64 in YAML)

**Dependencies**: Phase 3 (AudioNode interface)

**Verification**: VST3 plugins load, process audio, display UIs, save/restore state.
Parameter finder overlay works. yabridge-hosted plugins function correctly.

---

### Phase 5 — Final Cleanup

**Goal**: Remove JUCE from CMakeLists.txt, delete `libs/JUCE`, remove all
`#include <JuceHeader.h>`.

**Deliverables**:
- CMakeLists.txt: remove `juce_add_gui_app`, all `juce::juce_*` link targets
- Remove `libs/JUCE` submodule
- Remove `scripts/juce-patches/`
- Remove `JuceHeader.h` generation
- Clean build from scratch succeeds
- All `grep -r "juce::" src/` returns zero hits

**Dependencies**: All previous phases complete

**Verification**: Clean build. App launches. All features functional. No JUCE symbols
in binary (`nm` / `objdump` check).

---

## Dependency Graph

```
Phase 0 (Foundation)
   ├──→ Phase 1 (Model + Undo)
   │       └──→ Phase 3 (Audio Graph) ──→ Phase 4 (Plugins) ──→ Phase 5 (Cleanup)
   └──→ Phase 2 (Audio I/O + MIDI) ──→ Phase 3
```

Phases 1 and 2 can execute in parallel after Phase 0 completes.

## Worktree Strategy

Each phase gets its own worktree branching from `feature/sans-JUCE`:

| Worktree | Branch | Phase |
|----------|--------|-------|
| `feature/sans-JUCE` | `feature/sans-JUCE` | Coordination, docs |
| `feature/sans-juce-foundation` | `feature/sans-juce-foundation` | Phase 0 |
| `feature/sans-juce-model` | `feature/sans-juce-model` | Phase 1 |
| `feature/sans-juce-audio-io` | `feature/sans-juce-audio-io` | Phase 2 |
| `feature/sans-juce-graph` | `feature/sans-juce-graph` | Phase 3 |
| `feature/sans-juce-plugins` | `feature/sans-juce-plugins` | Phase 4 |

Merge flow: foundation → model + audio-io → graph → plugins → sans-JUCE (cleanup)

## Third-Party Library Decisions

| Library | Version | Purpose | Rationale |
|---------|---------|---------|-----------|
| **PortAudio** | v19.7+ | Audio device I/O | Cross-platform, mature, used by Audacity. Abstraction layer allows future swap to native CoreAudio/ALSA. |
| **libsndfile** | 1.2+ | Audio file read/write | Industry standard, supports WAV/AIFF/FLAC/OGG. Single C API. |
| **RtMidi** | 6.0+ | MIDI device I/O | Lightweight, cross-platform, header-only option. CoreMIDI/ALSA backends. |
| **VST3 SDK** | 3.7+ | Plugin hosting | Steinberg's official SDK. Already indirectly used via JUCE; going direct eliminates the abstraction layer and JUCE patches. |
| **Skia** | (existing) | GPU rendering | Already integrated. No change. |
| **yaml-cpp** | (existing) | Serialization | Already integrated. No change. |

**Not adopted**:
- **CLAP**: Excluded from scope — VST3-only simplifies the plugin hosting layer
- **AU (AudioUnit)**: Excluded — VST3-only. macOS users can use VST3 versions of plugins.
- **miniaudio**: Considered for audio I/O but PortAudio has better device enumeration and is more battle-tested for DAW use.

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| VST3 SDK complexity exceeds estimate | High | Medium | Start with JUCE's VST3 hosting code as reference. Leverage existing `PluginHost.cpp` patterns. |
| PortAudio latency issues on Linux | Medium | Low | Abstract behind `dc::AudioDeviceManager` interface; swap to native ALSA later if needed. |
| PropertyTree performance regression | Medium | Low | Benchmark against ValueTree early. PropertyTree is simpler (no XML overhead). |
| Plugin state binary compatibility | High | Low | Use identical binary format (getStateInformation blob). Test with existing sessions. |
| yabridge compatibility with direct VST3 hosting | High | Medium | yabridge exposes standard VST3 interfaces; test early with bridged plugins. |
| Parallel audio graph scheduling bugs | High | Medium | Start single-threaded, add parallelism incrementally. Extensive testing with complex plugin chains. |
| MIDI timing jitter with RtMidi | Medium | Low | RtMidi is well-proven. Use hardware timestamps where available. |
| Build time increase (VST3 SDK) | Low | Medium | VST3 SDK compiles quickly. Pre-compiled headers if needed. |

## Success Criteria

| Criterion | Measurement |
|-----------|-------------|
| Zero JUCE symbols | `grep -r "juce::" src/` returns 0 hits |
| Zero JUCE includes | `grep -r "JuceHeader" src/` returns 0 hits |
| No JUCE in CMake | No `juce_` targets in CMakeLists.txt |
| Session compatibility | Existing YAML sessions load correctly |
| Plugin compatibility | All previously-working VST3 plugins load and process |
| Audio quality | Bit-identical output for same session (offline bounce) |
| Latency | Round-trip latency within 10% of JUCE baseline |
| Binary size | Smaller than JUCE-linked binary |
| Build time | Comparable or faster than JUCE build |
| Parameter finder | Spatial hint overlay works identically |

## Document Index

| # | Document | Scope |
|---|----------|-------|
| 00 | [00-prd.md](00-prd.md) | This document — master plan |
| 01 | [01-observable-model.md](01-observable-model.md) | PropertyTree, PropertyId, Variant, listeners |
| 02 | [02-audio-graph.md](02-audio-graph.md) | AudioNode, AudioGraph, BufferPool, GraphExecutor |
| 03 | [03-plugin-hosting.md](03-plugin-hosting.md) | VST3Host, PluginInstance, PluginScanner, PluginEditor |
| 04 | [04-foundation-types.md](04-foundation-types.md) | Type mapping, MessageQueue, ListenerList, macros |
| 05 | [05-audio-io.md](05-audio-io.md) | AudioDeviceManager, AudioFileReader, DiskStreamer |
| 06 | [06-midi-subsystem.md](06-midi-subsystem.md) | MidiMessage, MidiBuffer, MidiSequence, MidiDeviceManager |
| 07 | [07-undo-system.md](07-undo-system.md) | UndoManager, UndoAction, coalescing, transactions |
| 08 | [08-migration-guide.md](08-migration-guide.md) | File-by-file checklist, per-phase verification |
