# Agent: MIDI Device I/O (RtMidi) + Engine Migration

You are working on the `feature/sans-juce-audio-io` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the Sans-JUCE migration: implement MIDI device I/O using RtMidi
and migrate the MIDI engine.

## Context

Read these specs before starting:
- `docs/sans-juce/06-midi-subsystem.md` (MidiDeviceManager design, SPSC recording flow)
- `docs/sans-juce/08-migration-guide.md` (Phase 2 section)
- `src/engine/MidiEngine.h/.cpp` (current JUCE MidiInput/MidiInputCallback implementation)
- `src/dc/foundation/spsc_queue.h` (existing lock-free SPSC queue)

## Prerequisites

Ensure RtMidi is available:

```bash
sudo apt install librtmidi-dev
```

Add to `CMakeLists.txt`:

```cmake
pkg_check_modules(RTMIDI REQUIRED rtmidi)
target_link_libraries(DremCanvas PRIVATE ${RTMIDI_LIBRARIES})
target_include_directories(DremCanvas PRIVATE ${RTMIDI_INCLUDE_DIRS})
```

## Dependencies

This agent depends on Agent 01 (MidiMessage, MidiBuffer). If those files don't exist yet,
create stub headers with the interfaces from `06-midi-subsystem.md` and implement
against them.

## Deliverables

### New files

#### 1. src/dc/midi/MidiDeviceManager.h/.cpp

RtMidi wrapper.

- `MidiDeviceInfo{index, name, isInput}`
- `MidiInputCallback`: `handleMidiMessage(MidiMessage, double timestamp)`
- `getInputDevices()`, `getOutputDevices()`
- `openInput(deviceIndex, callback)`, `closeInput(deviceIndex)`
- `openOutput(deviceIndex)`, `sendMessage(deviceIndex, MidiMessage)`, `closeOutput(deviceIndex)`
- `closeAll()`
- Static `rtMidiCallback` bridges raw bytes → `dc::MidiMessage` → `MidiInputCallback`

### Migration

#### 2. src/engine/MidiEngine.h/.cpp

Full migration:

- `juce::MidiInputCallback` → `dc::MidiInputCallback`
- `juce::MidiInput` → `dc::MidiDeviceManager`
- `juce::CriticalSection` + `std::vector<MidiMessage>` → `dc::SPSCQueue<dc::MidiMessage>`
- `juce::MidiBuffer` → `dc::MidiBuffer` in processBlock
- Lock-free flow: RtMidi callback → `SPSCQueue.push()` ... audio thread → `SPSCQueue.pop()`

#### 3. src/model/MidiClip.h/.cpp

Replace `juce::MidiMessageSequence` with `dc::MidiSequence`.

- Update binary serialization (base64 in YAML) to new format
- Backward-compatible reader: detect old JUCE format by checking version header

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
