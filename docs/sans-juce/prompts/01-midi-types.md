# Agent: MIDI Core Types

You are working on the `feature/sans-juce-audio-io` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the Sans-JUCE migration: implement the core MIDI types.

## Context

Read these specs before starting:
- `docs/sans-juce/06-midi-subsystem.md` (full MIDI design)
- `docs/sans-juce/08-migration-guide.md` (Phase 2 section)
- `src/dc/foundation/spsc_queue.h` (existing SPSCQueue — reuse, don't reimplement)

## Deliverables

Create the following files under `src/dc/midi/`:

### 1. MidiMessage.h/.cpp

Compact MIDI message class.

- 3-byte inline storage for channel messages, heap-allocated sysex
- Factory methods: `noteOn`, `noteOff`, `controllerEvent`, `programChange`, `pitchWheel`,
  `channelPressure`, `aftertouch`, `allNotesOff`, `allSoundOff`
- Query methods: `isNoteOn`, `isNoteOff`, `isController`, `getChannel` (1-based),
  `getNoteNumber`, `getVelocity` (float 0-1), `getRawVelocity` (int 0-127), etc.
- Mutation: `setChannel`, `setNoteNumber`, `setVelocity`

### 2. MidiBuffer.h/.cpp

Flat byte buffer for audio-thread MIDI events.

- Storage layout per event: `[int32 sampleOffset][int16 size][uint8 data...]`
- `addEvent(MidiMessage, sampleOffset)`, `clear()`, `getNumEvents()`, `isEmpty()`
- Iterator with `Event{sampleOffset, message}` — range-for compatible

### 3. MidiSequence.h/.cpp

Sorted vector of `TimedMidiEvent` for model layer.

- `TimedMidiEvent{double timeInBeats, MidiMessage, int matchedPairIndex}`
- `addEvent` (maintains sorted order), `removeEvent`, `sort()`, `updateMatchedPairs()`
- `getEventsInRange(startBeats, endBeats)` returning `pair<int,int>` indices
- Binary serialization: `toBinary()` / `fromBinary()` with version header
- Format: `[uint32 version=1][uint32 numEvents][per event: double time, uint16 size, bytes]`
- Backward-compatible reader that detects old JUCE format by checking version header

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
