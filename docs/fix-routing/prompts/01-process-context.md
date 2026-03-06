# Agent: ProcessContext Builder

You are working on the `feature/fix-routing` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to add tempo/time-signature fields to `TransportController` and create a `ProcessContextBuilder` that populates `Steinberg::Vst::ProcessContext` from transport state, with integration tests.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, testing)
- `src/engine/TransportController.h` (current atomic fields: position, sampleRate, playing, loop, record — NO tempo or time-sig)
- `src/engine/TransportController.cpp` (implementation pattern: atomic load/store, audio-thread safe)
- `src/dc/plugins/PluginInstance.cpp` lines 647-656 (where `processData_.processContext = nullptr` — this is what we're fixing)
- `tests/integration/test_audio_graph.cpp` (Catch2 test patterns: `TestBuffer`, `CHECK()`, `WithinAbs()`)

## Deliverables

### New files (`src/dc/plugins/`)

#### 1. ProcessContextBuilder.h

Static helper that populates a VST3 ProcessContext from TransportController.

```cpp
#pragma once
#include <pluginterfaces/vst/ivstprocesscontext.h>

namespace dc { class TransportController; }

namespace dc
{

struct ProcessContextBuilder
{
    // Populate ctx from transport state. Called per-block from PluginInstance::process().
    // Must be lock-free (reads atomics only).
    static void populate (Steinberg::Vst::ProcessContext& ctx,
                          const TransportController& transport,
                          int numSamples);
};

} // namespace dc
```

#### 2. ProcessContextBuilder.cpp

Implementation:
- `ctx.state` flags: always set `kTempoValid | kTimeSigValid | kProjectTimeMusicValid | kBarPositionValid | kSystemTimeValid`. Add `kPlaying` when `transport.isPlaying()`.
- `ctx.sampleRate` = `transport.getSampleRate()`
- `ctx.tempo` = `transport.getTempo()`
- `ctx.timeSigNumerator` = `transport.getTimeSigNumerator()`
- `ctx.timeSigDenominator` = `transport.getTimeSigDenominator()`
- `ctx.projectTimeSamples` = `transport.getPositionInSamples()`
- `ctx.projectTimeMusic` (PPQ) = `(positionInSamples / sampleRate) * (tempo / 60.0)` — quarter notes since start
- `ctx.barPositionMusic` = quantize PPQ to bar boundaries: `floor(ppq / beatsPerBar) * beatsPerBar` where `beatsPerBar = 4.0 * timeSigNumerator / timeSigDenominator`
- `ctx.systemTime` = 0 (not needed for correctness)
- `ctx.cycleStartMusic` / `ctx.cycleEndMusic` = 0.0 (loop support can come later)

Include `"engine/TransportController.h"` for the transport API.

### Modified files

#### 3. `src/engine/TransportController.h`

Add three new atomic members after the existing `sampleRate` atomic (line 54):

```cpp
std::atomic<double> tempo { 120.0 };
std::atomic<int> timeSigNumerator { 4 };
std::atomic<int> timeSigDenominator { 4 };
```

Add getters/setters in the public section (after `setSampleRate`, line 32):

```cpp
double getTempo() const { return tempo.load(); }
void setTempo (double bpm) { tempo.store (bpm); }
int getTimeSigNumerator() const { return timeSigNumerator.load(); }
int getTimeSigDenominator() const { return timeSigDenominator.load(); }
void setTimeSig (int num, int den) { timeSigNumerator.store (num); timeSigDenominator.store (den); }
```

### New test file

#### 4. `tests/integration/test_plugin_process_context.cpp`

Catch2 integration tests for ProcessContextBuilder. Follow the pattern in `test_audio_graph.cpp`.

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dc/plugins/ProcessContextBuilder.h"
#include "engine/TransportController.h"
#include <pluginterfaces/vst/ivstprocesscontext.h>
```

Test cases:

- **"ProcessContextBuilder: populates tempo from transport"** `[integration][plugin]`
  - Set transport: sampleRate=44100, tempo=140, timeSig=3/4, playing, position=44100 samples
  - Call `ProcessContextBuilder::populate(ctx, transport, 512)`
  - CHECK: `ctx.tempo == 140.0`, `ctx.timeSigNumerator == 3`, `ctx.timeSigDenominator == 4`
  - CHECK: `ctx.sampleRate == 44100.0`, `ctx.projectTimeSamples == 44100`
  - CHECK: `(ctx.state & kPlaying) != 0`
  - CHECK: `(ctx.state & kTempoValid) != 0`, `(ctx.state & kTimeSigValid) != 0`

- **"ProcessContextBuilder: stopped transport has no kPlaying flag"** `[integration][plugin]`
  - Set transport: sampleRate=44100, tempo=120, NOT playing
  - CHECK: `(ctx.state & kPlaying) == 0`
  - CHECK: `(ctx.state & kTempoValid) != 0` (tempo is still valid even when stopped)

- **"ProcessContextBuilder: PPQ position calculation"** `[integration][plugin]`
  - Set transport: sampleRate=44100, tempo=120, position=44100 samples (1 second)
  - At 120 BPM: 1 second = 2 quarter notes
  - CHECK_THAT: `ctx.projectTimeMusic` is `WithinAbs(2.0, 0.001)`

- **"ProcessContextBuilder: bar position quantization"** `[integration][plugin]`
  - Set transport: sampleRate=44100, tempo=120, timeSig=4/4, position=132300 (3 seconds = 6 beats)
  - Bar position should be beat 4 (start of bar 2, since bar 1 is beats 0-3)
  - CHECK_THAT: `ctx.barPositionMusic` is `WithinAbs(4.0, 0.001)`

### Build integration

#### 5. `CMakeLists.txt` (root)

Add after the existing `src/dc/plugins/VST3Host.cpp` entry (~line 201):
```
src/dc/plugins/ProcessContextBuilder.cpp
```

#### 6. `tests/CMakeLists.txt`

Add `test_plugin_process_context.cpp` to `dc_integration_tests` sources (after line 120).

The integration test target already links `Catch2::Catch2WithMain` and includes the necessary paths. However, this test includes VST3 SDK headers (`pluginterfaces/vst/ivstprocesscontext.h`). Check whether the integration test target has access to VST3 SDK include paths — if not, add:
```cmake
target_include_directories(dc_integration_tests PRIVATE ${VST3_SDK_DIR})
```

Also add `src/dc/plugins/ProcessContextBuilder.cpp` to the integration test app-layer sources so it compiles in the test target.

## Audio Thread Safety

`ProcessContextBuilder::populate()` is called from the audio thread inside `PluginInstance::process()`. It must:
- Only read `std::atomic` fields from TransportController (no locks, no allocation)
- Not call any non-lock-free methods
- Not use `std::cout`, `new`, `delete`, `malloc`, `std::mutex`, or `pthread_create`

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"engine/TransportController.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
