# Agent: Parameter Changes + MIDI CC Mapper

You are working on the `feature/fix-routing` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to implement `ParameterChangeQueue` (VST3 `IParameterChanges` + `IParamValueQueue`) and `MidiCCMapper` (translates MIDI CC to VST3 parameter changes), with unit and integration tests.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, testing)
- `src/dc/plugins/PluginInstance.cpp` lines 647-716 (where `inputParameterChanges = nullptr` and CC/pitch bend are skipped)
- `src/dc/midi/MidiMessage.h` (factory methods and query API: `isController()`, `getControllerNumber()`, `getControllerValue()`, `isPitchWheel()`, `getPitchWheelValue()`)
- `tests/unit/plugins/test_plugin_description.cpp` (Catch2 unit test pattern)
- `tests/integration/test_audio_graph.cpp` (integration test pattern with `WithinAbs`)

VST3 SDK interfaces to implement against (in `build/_deps/vst3sdk-src/pluginterfaces/vst/`):
- `ivstparameterchanges.h` — `IParameterChanges`, `IParamValueQueue`
- `ivstmidicontrollers.h` — `IMidiMapping` (has `getMidiControllerAssignment`)
- `ControllerNumbers` enum in `ivstmidicontrollers.h` — `kPitchBend = 129`, `kAfterTouch = 128`

## Deliverables

### New files (`src/dc/plugins/`)

#### 1. ParameterChangeQueue.h

Implements `Steinberg::Vst::IParameterChanges`. Pre-allocated, no heap allocation during `process()`.

```cpp
#pragma once
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <array>

namespace dc
{

class ParamValueQueue : public Steinberg::Vst::IParamValueQueue
{
public:
    ParamValueQueue();

    void setParameterId (Steinberg::Vst::ParamID id);
    void clear();

    // IParamValueQueue
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override;
    Steinberg::int32 PLUGIN_API getPointCount() override;
    Steinberg::tresult PLUGIN_API getPoint (Steinberg::int32 index,
        Steinberg::int32& sampleOffset,
        Steinberg::Vst::ParamValue& value) override;
    Steinberg::tresult PLUGIN_API addPoint (Steinberg::int32 sampleOffset,
        Steinberg::Vst::ParamValue value,
        Steinberg::int32& index) override;

    // FUnknown (minimal — no ref counting needed for stack/member objects)
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID, void**) override
    { return Steinberg::kNoInterface; }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }

private:
    static constexpr int kMaxPoints = 16;
    Steinberg::Vst::ParamID paramId_ = 0;
    int pointCount_ = 0;
    struct Point { Steinberg::int32 sampleOffset; Steinberg::Vst::ParamValue value; };
    std::array<Point, kMaxPoints> points_ {};
};

class ParameterChangeQueue : public Steinberg::Vst::IParameterChanges
{
public:
    ParameterChangeQueue();

    void clear();

    // IParameterChanges
    Steinberg::int32 PLUGIN_API getParameterCount() override;
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData (Steinberg::int32 index) override;
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData (
        const Steinberg::Vst::ParamID& id,
        Steinberg::int32& index) override;

    // FUnknown (minimal)
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID, void**) override
    { return Steinberg::kNoInterface; }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }

private:
    static constexpr int kMaxParams = 128;
    int paramCount_ = 0;
    std::array<ParamValueQueue, kMaxParams> queues_;
};

} // namespace dc
```

#### 2. ParameterChangeQueue.cpp

Implementation notes:
- `addParameterData`: scan existing queues for matching `paramId`; if found, return it with its index. Otherwise allocate next slot, set paramId, return it. Return `nullptr` if full.
- `clear()`: reset `paramCount_` to 0, call `clear()` on each used queue.
- `ParamValueQueue::addPoint`: append to points array if not full. Set `index` to the new point index. Return `kResultOk`.
- `ParamValueQueue::getPoint`: bounds-check, copy sampleOffset and value out. Return `kResultOk` or `kResultFalse`.

#### 3. MidiCCMapper.h

Maps MIDI CC numbers to VST3 ParamIDs using the plugin's `IMidiMapping` interface.

```cpp
#pragma once
#include "ParameterChangeQueue.h"
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <array>

namespace dc
{

class MidiMessage;

class MidiCCMapper
{
public:
    MidiCCMapper();

    // Query IMidiMapping from controller and cache all CC->ParamID mappings.
    // Call once at plugin load time (message thread). Safe if controller is null
    // or does not implement IMidiMapping.
    void buildFromController (Steinberg::Vst::IEditController* controller);

    // Translate a MIDI CC or pitch bend message to a parameter change.
    // Called from audio thread — must be lock-free.
    // Returns true if a mapping was found and a point was added.
    bool translateToParameterChanges (const MidiMessage& msg,
                                      int sampleOffset,
                                      ParameterChangeQueue& queue) const;

    // Check if any mappings were found
    bool hasMappings() const { return hasMappings_; }

private:
    // 128 standard CCs + kAfterTouch(128) + kPitchBend(129) + kCountCtrlNumber(130)
    static constexpr int kNumControllers = 131;
    static constexpr Steinberg::Vst::ParamID kUnmapped = 0xFFFFFFFF;

    std::array<Steinberg::Vst::ParamID, kNumControllers> ccToParam_;
    bool hasMappings_ = false;
};

} // namespace dc
```

#### 4. MidiCCMapper.cpp

Implementation notes:
- `buildFromController`: Query `IMidiMapping` via `controller->queryInterface()`. If not supported, return (hasMappings_ stays false). Otherwise iterate CC 0-127 plus `Steinberg::Vst::ControllerNumbers::kAfterTouch` (128) and `kPitchBend` (129), calling `getMidiControllerAssignment(0, 0, cc, &paramId)` for each. Store results in `ccToParam_` array.
- `translateToParameterChanges`:
  - If `msg.isController()`: look up `ccToParam_[msg.getControllerNumber()]`. If mapped, add point with value `msg.getControllerValue() / 127.0`.
  - If `msg.isPitchWheel()`: look up `ccToParam_[129]` (kPitchBend). If mapped, add point with value `msg.getPitchWheelValue() / 16383.0`.
  - If `msg.isChannelPressure()`: look up `ccToParam_[128]` (kAfterTouch). If mapped, add point with value normalized.
  - Use `queue.addParameterData(paramId, index)` then `queue->addPoint(sampleOffset, value, pointIndex)`.
  - Return false if `ccToParam_` entry is `kUnmapped`.

Include `"dc/midi/MidiMessage.h"` for the MidiMessage API.

### New test files

#### 5. `tests/integration/test_parameter_changes.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "dc/plugins/ParameterChangeQueue.h"
```

Test cases:

- **"ParameterChangeQueue: addParameterData creates entries"** `[integration][plugin]`
  - Create queue, call `addParameterData(42, index)`.
  - CHECK: returned pointer is not null, index == 0, `getParameterCount() == 1`

- **"ParameterChangeQueue: same param reuses existing entry"** `[integration][plugin]`
  - Add param 42 twice. CHECK: both return same index, `getParameterCount() == 1`

- **"ParameterChangeQueue: multiple params tracked separately"** `[integration][plugin]`
  - Add params 42 and 99. CHECK: different indices, `getParameterCount() == 2`

- **"ParameterChangeQueue: clear resets state"** `[integration][plugin]`
  - Add params, call `clear()`. CHECK: `getParameterCount() == 0`

- **"ParamValueQueue: addPoint and getPoint round-trip"** `[integration][plugin]`
  - Create queue, set paramId, add point (offset=100, value=0.5).
  - CHECK: `getPointCount() == 1`, `getPoint(0, ...)` returns offset=100 and value within 0.001 of 0.5

#### 6. `tests/unit/plugins/test_cc_to_param_translation.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dc/plugins/MidiCCMapper.h"
#include "dc/midi/MidiMessage.h"
```

Since we can't easily instantiate a real `IEditController` in unit tests, test `translateToParameterChanges` by manually populating the mapper's internal state. Add a test-only method or make `ccToParam_` accessible for testing. Preferred approach: add a public method:

```cpp
// For testing: manually add a CC mapping
void addMapping (int ccNumber, Steinberg::Vst::ParamID paramId);
```

Test cases:

- **"MidiCCMapper: controller message translates to parameter change"** `[unit][plugin]`
  - Create mapper, call `addMapping(1, 100)` (CC1 -> ParamID 100)
  - Create `MidiMessage::controllerEvent(1, 1, 64)` (ch1, CC1, value 64)
  - Call `translateToParameterChanges(msg, 0, queue)`
  - CHECK: `queue.getParameterCount() == 1`, returned true
  - CHECK: `getParameterData(0)->getParameterId() == 100`

- **"MidiCCMapper: unmapped CC is ignored"** `[unit][plugin]`
  - Create mapper with no mappings
  - Translate CC1 message
  - CHECK: `queue.getParameterCount() == 0`, returned false

- **"MidiCCMapper: pitch bend translates to kPitchBend mapping"** `[unit][plugin]`
  - Create mapper, call `addMapping(129, 200)` (kPitchBend -> ParamID 200)
  - Create `MidiMessage::pitchWheel(1, 8192)` (center value)
  - CHECK: queue has 1 parameter, value is approximately 8192.0/16383.0

- **"MidiCCMapper: CC value normalized to 0-1 range"** `[unit][plugin]`
  - Map CC7 -> ParamID 50, send CC value 127
  - CHECK: point value is approximately 1.0
  - Send CC value 0, CHECK: point value is approximately 0.0

### Build integration

#### 7. `CMakeLists.txt` (root)

Add after the existing `src/dc/plugins/` entries (~line 201):
```
src/dc/plugins/ParameterChangeQueue.cpp
src/dc/plugins/MidiCCMapper.cpp
```

#### 8. `tests/CMakeLists.txt`

Add to `dc_integration_tests` sources: `integration/test_parameter_changes.cpp`
Add to `dc_unit_tests` sources: `unit/plugins/test_cc_to_param_translation.cpp`

Both test targets need VST3 SDK include paths. Check if `${VST3_SDK_DIR}` is already in include directories for these targets. If not, add:
```cmake
target_include_directories(dc_unit_tests PRIVATE ${VST3_SDK_DIR})
target_include_directories(dc_integration_tests PRIVATE ${VST3_SDK_DIR})
```

Add `ParameterChangeQueue.cpp` and `MidiCCMapper.cpp` to the test targets' app-layer sources so they compile in test builds.

## Audio Thread Safety

`MidiCCMapper::translateToParameterChanges()` and all `ParameterChangeQueue` methods are called from the audio thread. They must:
- Not allocate (`new`, `malloc`)
- Not lock (`std::mutex`, `pthread_mutex`)
- Not use `std::cout`, `std::string`, or any throwing operations
- Use only fixed-size arrays and simple arithmetic

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"dc/midi/MidiMessage.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
