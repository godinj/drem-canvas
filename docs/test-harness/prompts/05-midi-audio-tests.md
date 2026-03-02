# Agent: MIDI & Audio Tests

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phases 5–6 of the test harness: write unit tests for all dc::midi and
dc::audio types that replaced JUCE equivalents.

## Context

Read these specs before starting:

- `docs/test-harness/02-migration-tests.md` (Phase 2 — MIDI and Audio Tests sections)
- `docs/test-harness/00-prd.md` (test directory layout, test fixtures)
- `src/dc/midi/MidiMessage.h` + `MidiMessage.cpp`
- `src/dc/midi/MidiBuffer.h` + `MidiBuffer.cpp`
- `src/dc/midi/MidiSequence.h` + `MidiSequence.cpp`
- `src/dc/audio/AudioBlock.h` (header-only)
- `src/dc/audio/AudioFileReader.h` + `AudioFileReader.cpp`
- `src/dc/audio/AudioFileWriter.h` + `AudioFileWriter.cpp`
- `src/dc/audio/DiskStreamer.h` + `DiskStreamer.cpp`
- `src/dc/audio/ThreadedRecorder.h` + `ThreadedRecorder.cpp`

Read each source file to understand the exact API before writing tests.

## Prerequisites

Phase 1 (CMake infrastructure) must be completed. The `dc_midi`, `dc_audio` library
targets and `dc_unit_tests` executable must exist.

## Deliverables

Create 7 test files. Add each to `target_sources(dc_unit_tests ...)` in
`tests/CMakeLists.txt`.

### MIDI Tests (tests/unit/midi/)

#### 1. tests/unit/midi/test_midi_message.cpp

Test `dc::MidiMessage`:
- All factory methods: `noteOn`, `noteOff`, `controllerEvent`, `programChange`,
  `pitchWheel`, `aftertouch`, `channelPressure`, `allNotesOff`, `allSoundOff`
- Channel clamping: 0→1, 17→16
- Velocity clamping: negative→0, >1.0→1.0
- Note clamping: negative→0, ≥128→127
- Pitch wheel range: 0–16383
- `isNoteOff()` returns true for velocity-0 noteOn (0x90 with vel=0)
- `getVelocity()` float [0,1] vs `getRawVelocity()` int [0,127]
- Raw data round-trip: construct from getRawData/getRawDataSize
- SysEx message: `isSysEx()` true, data stored in heap buffer
- Empty message: `getRawDataSize() == 0`
- `setChannel()`, `setNoteNumber()`, `setVelocity()` mutations

#### 2. tests/unit/midi/test_midi_buffer.cpp

Test `dc::MidiBuffer`:
- `addEvent()` then iterate — correct sampleOffset and message
- Multiple events at same offset — all preserved in insertion order
- `clear()` → `isEmpty()` true, `getNumEvents()` == 0
- Iterator reconstructs MidiMessage correctly from flat byte layout
- Negative sampleOffset accepted
- Large sampleOffset — no overflow
- SysEx event in buffer — variable-length stored correctly
- Empty buffer: `begin() == end()`

#### 3. tests/unit/midi/test_midi_sequence.cpp

Test `dc::MidiSequence`:
- `addEvent()` maintains sorted order by timeInBeats
- `updateMatchedPairs()` links noteOn to first unmatched noteOff (same note/channel)
- Unmatched noteOn: `matchedPairIndex` stays -1
- Multiple noteOff for same note: first unmatched matches
- `getEventsInRange(start, end)` returns correct index range
- Range with no events: empty range
- `removeEvent(index)` — event removed, indices shift
- `sort()` after manual manipulation
- Binary serialization round-trip: `fromBinary(seq.toBinary())`
- Legacy JUCE format deserialization (big-endian double + int32 header)
- `getEvents()` returns const reference

### Audio Tests (tests/unit/audio/)

#### 4. tests/unit/audio/test_audio_block.cpp

Test `dc::AudioBlock`:
- `getChannel(ch)` returns correct pointer
- `getNumChannels()` / `getNumSamples()` match constructor args
- `clear()` zeros all samples in all channels
- Zero channels / zero samples — valid, no crash
- Const correctness: `const AudioBlock` exposes only `const float*`

#### 5. tests/unit/audio/test_audio_file_io.cpp

Test `dc::AudioFileReader` + `dc::AudioFileWriter`:
- Write WAV (16-bit, 24-bit, 32-float), read back, compare samples
- Write AIFF (16-bit, 24-bit), read back, compare
- Write FLAC (16-bit, 24-bit), read back, compare
- AudioBlock interleave/de-interleave round-trip
- `open()` non-existent file returns nullptr
- `open()` non-audio file returns nullptr
- Read past end of file: returns fewer frames
- Multi-channel: mono, stereo
- `getFormatName()`, `getBitDepth()` return correct metadata

Use temporary files (create in a temp directory, clean up after test). Generate
test audio programmatically (e.g., fill buffer with known sine wave or ramp values).

#### 6. tests/unit/audio/test_disk_streamer.cpp

Test `dc::DiskStreamer`:
- `open()` + `start()` + `read()` returns correct audio data
- `read()` before `start()` returns silence
- `seek()` to middle of file, subsequent reads start from there
- `seek()` past EOF: clamps, reads return 0 frames
- Sequential `open()` calls: previous file closed automatically
- Small buffer size (1 frame): still works
- `getLengthInSamples()`, `getSampleRate()`, `getNumChannels()` match source

These tests require writing a temp WAV file first, then streaming from it.

#### 7. tests/unit/audio/test_threaded_recorder.cpp

Test `dc::ThreadedRecorder`:
- Record audio, stop, verify output file contains submitted samples
- `write()` never blocks (returns immediately)
- Overflow: write faster than disk — samples dropped, no crash
- `stop()` without `start()`: no crash
- `stop()` flushes remaining buffered data

## Prerequisites for Audio Tests

Audio file I/O tests require `libsndfile` to be available. The `dc_audio` target
must link it correctly.

Generate test fixtures programmatically in the test setup rather than relying on
pre-existing fixture files. This makes tests self-contained:

```cpp
// Helper to create a test WAV file
std::filesystem::path createTestWav(int numChannels, int sampleRate, int numFrames)
{
    auto path = std::filesystem::temp_directory_path() / "dc_test.wav";
    auto writer = dc::AudioFileWriter::open(path, numChannels, sampleRate,
                                             dc::AudioFileFormat::WAV_16);
    // Fill with ramp values
    std::vector<float> buffer(numFrames * numChannels);
    for (int i = 0; i < numFrames * numChannels; ++i)
        buffer[i] = float(i) / float(numFrames * numChannels);
    writer->write(buffer.data(), numFrames);
    return path;
}
```

## Important

- Read the actual source code before writing tests. The spec provides expected
  invariants, but the real API may differ in details.
- MIDI tests are pure logic — no system dependencies, no hardware.
- Audio file tests need libsndfile but no audio hardware.
- DiskStreamer and ThreadedRecorder tests involve background threads — use appropriate
  synchronization (e.g., wait for streamer to fill buffers before asserting).
- Never include `<JuceHeader.h>` in test files.
- For floating-point audio sample comparisons, use appropriate tolerances based on
  bit depth (16-bit: ~1/32768, 24-bit: ~1/8388608, 32-float: exact).

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `tests/CMakeLists.txt` `target_sources(dc_unit_tests ...)`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure`
