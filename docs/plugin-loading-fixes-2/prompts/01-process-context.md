# Agent: Provide ProcessContext to VST3 Plugins

You are working on the `feature/plugin-loading-fixes` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is populating the `Steinberg::Vst::ProcessContext`
struct in `PluginInstance::process()` so tempo-synced plugins receive correct transport state.

## Context

Read these before starting:
- `src/dc/plugins/PluginInstance.cpp` (line 594: `process()` — `processData_.processContext = nullptr` at line 630)
- `src/dc/plugins/PluginInstance.h` (line 90: `processData_` member, line 110-112: `currentSampleRate_`, `currentBlockSize_`, `prepared_`)
- `src/engine/TransportController.h` (full file — all atomics: `playing`, `positionInSamples`, `sampleRate`, `loopEnabled`, `loopStartInSamples`, `loopEndInSamples`)
- `src/engine/AudioEngine.h` (line 19: `getGraph()` — how audio engine wraps the graph)
- `src/engine/AudioEngine.cpp` (the `GraphCallback::audioCallback` — where transport position advances)
- `src/model/TempoMap.h` / `.cpp` (tempo and time signature storage)

Also check the VST3 SDK header for the struct definition:
- `build/_deps/vst3sdk-src/pluginterfaces/vst/ivstprocesscontext.h` (ProcessContext fields and flag constants)

## Problem

`PluginInstance::process()` sets `processData_.processContext = nullptr`. Many VST3 plugins
(particularly synths and tempo-synced effects like Phase Plant, delay, LFO, arpeggiator)
read tempo, time signature, and transport state from this struct. With yabridge, null
`processContext` can cause crashes or incorrect behavior in Windows plugins.

## Deliverables

### Modified files

#### 1. `src/dc/plugins/PluginInstance.h`

Add a `Steinberg::Vst::ProcessContext` member alongside the existing `processData_`:

```cpp
// Audio processing state
Steinberg::Vst::ProcessData processData_ {};
Steinberg::Vst::ProcessContext processContext_ {};  // NEW
```

Add a method to update transport state from the audio thread:

```cpp
/// Update transport state before processing. Called from audio thread.
void setTransportState (int64_t positionInSamples, double sampleRate,
                        double tempo, int timeSigNumerator, int timeSigDenominator,
                        bool isPlaying, bool isLooping,
                        int64_t loopStartSamples, int64_t loopEndSamples);
```

#### 2. `src/dc/plugins/PluginInstance.cpp`

Implement `setTransportState`:

```cpp
void PluginInstance::setTransportState (int64_t positionInSamples, double sampleRate,
                                        double tempo, int timeSigNumerator,
                                        int timeSigDenominator, bool isPlaying,
                                        bool isLooping, int64_t loopStartSamples,
                                        int64_t loopEndSamples)
{
    using namespace Steinberg::Vst;

    processContext_.state = 0;
    if (isPlaying)
        processContext_.state |= ProcessContext::kPlaying;
    if (tempo > 0.0)
        processContext_.state |= ProcessContext::kTempoValid;
    if (timeSigNumerator > 0)
        processContext_.state |= ProcessContext::kTimeSigValid;
    if (isLooping)
    {
        processContext_.state |= ProcessContext::kCycleActive;
        processContext_.cycleStartMusic = static_cast<double> (loopStartSamples) / sampleRate * (tempo / 60.0);
        processContext_.cycleEndMusic   = static_cast<double> (loopEndSamples)   / sampleRate * (tempo / 60.0);
    }

    processContext_.state |= ProcessContext::kProjectTimeMusicValid;
    processContext_.state |= ProcessContext::kSystemTimeValid;

    processContext_.sampleRate = sampleRate;
    processContext_.projectTimeSamples = positionInSamples;
    processContext_.tempo = tempo;
    processContext_.timeSigNumerator = timeSigNumerator;
    processContext_.timeSigDenominator = timeSigDenominator;

    // Convert samples to musical position (quarter notes from start)
    if (tempo > 0.0 && sampleRate > 0.0)
        processContext_.projectTimeMusic = static_cast<double> (positionInSamples) / sampleRate * (tempo / 60.0);
    else
        processContext_.projectTimeMusic = 0.0;

    processContext_.systemTime = 0;  // Not used currently
}
```

In `process()`, change line 630 from:

```cpp
processData_.processContext = nullptr;
```

to:

```cpp
processData_.processContext = &processContext_;
```

#### 3. Call site: audio engine callback

Find where `PluginInstance::process()` is called via the audio graph and ensure
`setTransportState()` is called on each plugin before its `process()`. The cleanest
approach:

**Option A (preferred)**: Add transport state to the `GraphExecutor::execute()` call
by having `AudioEngine::GraphCallback::audioCallback` call `setTransportState` on each
`PluginProcessorNode` before graph execution. Look at how `audioCallback` already reads
from `TransportController` to advance position.

**Option B**: Have `PluginInstance::process()` itself read transport state if a
`TransportController*` is stored as a member. This is simpler but couples the plugin
layer to the engine layer.

Choose whichever approach is cleanest given the existing architecture. The key constraint:
`setTransportState` must be called from the audio thread, before `process()`, every block.

#### 4. `tests/unit/plugins/test_plugin_instance.cpp` (NEW)

Unit test for `setTransportState` + `processContext_` population:

- Verify playing flag is set when `isPlaying = true`
- Verify tempo is stored and `kTempoValid` flag is set
- Verify time signature is stored and `kTimeSigValid` flag is set
- Verify `projectTimeMusic` is computed correctly (position / sampleRate * tempo / 60)
- Verify `kCycleActive` and cycle positions when looping is enabled
- Verify default state (all zeros/false) produces a valid struct with no flags

### Modified files

#### 5. `tests/CMakeLists.txt`

Add `tests/unit/plugins/test_plugin_instance.cpp` to the test target.

## Audio Thread Safety

- `setTransportState` is called from the audio thread — no allocations, no locks
- `processContext_` is a plain struct member, not shared — safe to write before `process()` reads
- All values come from `std::atomic` reads in `TransportController` — lock-free

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Run verification: `scripts/verify.sh`
