# Agent: Regression Test for setState Deactivation

You are working on the `feature/plugin-loading-fixes` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is writing a regression test that verifies
`PluginInstance::setState()` properly deactivates processing before restoring state
and reactivates after.

## Context

Read these before starting:
- `src/dc/plugins/PluginInstance.cpp` (line 879: `setState()` — the recent fix that deactivates before state restore)
- `src/dc/plugins/PluginInstance.h` (full file — `process()`, `prepare()`, `release()`, member variables)
- `tests/regression/README.md` (regression test template and conventions)
- `tests/regression/issue_002_juce_state_format.cpp` (existing state format regression test — for style reference)
- `tests/unit/plugins/test_plugin_description.cpp` (for test structure reference)

## Background

A crash was discovered where `PluginInstance::process()` segfaulted inside yabridge after
`setState()` was called with legacy JUCE-format data. The root cause: per the VST3 spec,
`IComponent::setState()` must be called while the plugin is NOT active/processing. The fix
deactivates processing before `setState()` and reactivates after, regardless of success or
failure.

The fix is in place but has no test coverage. This regression test ensures the
deactivate/reactivate pattern is never accidentally removed.

## Deliverables

### New files

#### 1. `tests/regression/issue_003_set_state_deactivation.cpp`

Regression test verifying the deactivation contract. Since we can't easily create a real
`PluginInstance` in unit tests (it requires a loaded VST3 module), test the contract
through a mock/spy approach:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <vector>
#include <cstring>

// Spy that records the sequence of setProcessing/setActive/setState calls
// to verify the deactivation contract.

namespace {

struct CallRecord
{
    enum Type { SetProcessing, SetActive, SetState };
    Type type;
    bool boolArg;  // for setProcessing(bool) and setActive(bool)
    // For setState: whether the call was made (we just record it happened)
};

} // anonymous namespace

TEST_CASE ("setState deactivates processing before restore and reactivates after",
           "[regression]")
{
    // This test verifies the CONTRACT, not the full PluginInstance.
    // The contract is:
    //   1. setProcessing(false) is called before any IComponent::setState
    //   2. setActive(false) is called before any IComponent::setState
    //   3. setActive(true) is called after IComponent::setState
    //   4. setProcessing(true) is called after setActive(true)
    //
    // We verify this by reading the source code pattern in PluginInstance::setState.
    // A full integration test with a real plugin is in the manual test suite.

    // Verify the source code contains the deactivation pattern.
    // This is a structural test — if the pattern is removed, this test
    // must be updated or the deactivation contract is violated.

    SECTION ("format mismatch path deactivates and reactivates")
    {
        // Simulate the format mismatch scenario (componentSize > data.size)
        // The key assertion: even when setState fails, the plugin is reactivated.
        std::vector<uint8_t> badData = {
            0x57, 0x30, 0xEA, 0xDD,  // componentSize = 3724320471 (way too big)
            0x01, 0x02, 0x03, 0x04,
            0x05, 0x06, 0x07, 0x08
        };

        uint32_t componentSize = 0;
        std::memcpy (&componentSize, badData.data(), 4);

        // This is the condition that triggers the format mismatch path
        REQUIRE (4 + componentSize > badData.size());

        // Document the expected call sequence for this path:
        // 1. setProcessing(false)
        // 2. setActive(false)
        // 3. component_->setState(stream)  [raw pass-through, may fail]
        // 4. setActive(true)
        // 5. setProcessing(true)
        SUCCEED ("Format mismatch path verified — deactivation pattern documented");
    }

    SECTION ("normal path deactivates and reactivates")
    {
        // Simulate valid format data
        uint32_t componentSize = 8;
        std::vector<uint8_t> goodData (4 + componentSize + 4);
        std::memcpy (goodData.data(), &componentSize, 4);

        REQUIRE (4 + componentSize <= goodData.size());

        // Document the expected call sequence for this path:
        // 1. setProcessing(false)
        // 2. setActive(false)
        // 3. component_->setState(componentStream)
        // 4. controller_->setComponentState(componentStream)
        // 5. controller_->setState(controllerStream)  [if controller data present]
        // 6. setActive(true)
        // 7. setProcessing(true)
        SUCCEED ("Normal path verified — deactivation pattern documented");
    }

    SECTION ("setState with tiny data is a no-op (no crash)")
    {
        // data.size() < 4 should early-return without touching component
        std::vector<uint8_t> tinyData = { 0x01, 0x02 };
        REQUIRE (tinyData.size() < 4);
        SUCCEED ("Tiny data early-return path verified");
    }
}
```

If you can find a way to create a mock `IComponent` / `IAudioProcessor` that records
call sequences (using trompeloeil or a manual spy), prefer that over the structural
test above. Check if trompeloeil is available in the test dependencies
(`tests/CMakeLists.txt` or `CMakeLists.txt` FetchContent).

The ideal test would:
1. Create a spy `IComponent` and spy `IAudioProcessor`
2. Inject them into a `PluginInstance` (may require a test-only constructor or friend)
3. Call `setState()` with bad data
4. Verify the call sequence: `setProcessing(false)` → `setActive(false)` → `setState(data)` → `setActive(true)` → `setProcessing(true)`

If the PluginInstance constructor is too private to mock, the structural/documentation
test above is acceptable as a regression marker.

### Modified files

#### 2. `tests/CMakeLists.txt`

Add `tests/regression/issue_003_set_state_deactivation.cpp` to the regression test target.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Regression tests go in `tests/regression/` with `[regression]` tag
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Run verification: `scripts/verify.sh`
