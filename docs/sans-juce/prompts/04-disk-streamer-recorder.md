# Agent: DiskStreamer + ThreadedRecorder

You are working on the `feature/sans-juce-audio-io` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the Sans-JUCE migration: implement background disk I/O for
streaming and recording.

## Context

Read these specs before starting:
- `docs/sans-juce/05-audio-io.md` (DiskStreamer, ThreadedRecorder design)
- `docs/sans-juce/08-migration-guide.md` (Phase 2 section)
- `src/dc/foundation/spsc_queue.h` (existing lock-free SPSC ring buffer — use this)
- `src/engine/AudioRecorder.h/.cpp` (current JUCE ThreadedWriter-based recorder)

## Dependencies

This agent depends on Agent 02 (AudioFileReader/Writer). If those files don't exist yet,
create stub headers with the interfaces from `05-audio-io.md` and implement against them.

## Deliverables

### New files (src/dc/audio/)

#### 1. DiskStreamer.h/.cpp

Background-threaded disk reader with ring buffer.

- `open(fs::path)` → `bool`, `close()`, `seek(int64_t positionInSamples)`
- `read(AudioBlock& output, int numSamples)` → `int` (called from audio thread, non-blocking)
- `start()` / `stop()` for the background read thread
- `getLengthInSamples()`, `getSampleRate()`, `getNumChannels()`
- Internal: `SPSCQueue<float>` ring buffer (~1 second ahead), background thread fills it
- On seek: set atomic `seekTarget_`, background thread detects and repositions
- On underrun: output silence, log warning

#### 2. ThreadedRecorder.h/.cpp

Lock-free audio recording to disk.

- `start(path, AudioFileWriter::Format, numChannels, sampleRate)` → `bool`
- `write(const AudioBlock& block, int numSamples)` — audio thread, lock-free push to ring
- `stop()` — flushes remaining data, closes file
- `isRecording()`
- Internal: `SPSCQueue<float>` ring buffer, background thread drains to `AudioFileWriter`

### Migration

#### 3. src/engine/AudioRecorder.h/.cpp

Replace `juce::AudioFormatWriter::ThreadedWriter` + `juce::TimeSliceThread` with
`dc::ThreadedRecorder`. Keep the same public interface that `TrackProcessor` and
`BounceProcessor` use.

## Audio Thread Safety Rules

- `write()` and `read()` are called from the audio thread — NEVER allocate or lock
- Use `dc::SPSCQueue` for all audio-to-background thread communication
- Background threads may allocate (disk I/O), audio thread must not

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
