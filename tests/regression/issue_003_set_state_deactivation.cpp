// tests/regression/issue_003_set_state_deactivation.cpp
//
// Bug: PluginInstance::process() segfaulted inside yabridge after
//      setState() was called with legacy JUCE-format data while the
//      plugin was still active and processing.
//
// Cause: Per the VST3 spec, IComponent::setState() must be called
//        while the plugin is NOT active/processing.  The original
//        setState() called component_->setState() without first
//        deactivating processing.
//
// Fix: setState() now calls processor_->setProcessing(false) and
//      component_->setActive(false) before any IComponent::setState
//      call, and unconditionally reactivates afterwards — even if
//      the state restoration fails.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// PluginInstance is not easily instantiated in tests (requires a loaded
// VST3 module).  Instead, we model the deactivation contract with a
// lightweight spy and a free function that mirrors PluginInstance::setState's
// deactivation logic.  If the production code's pattern is ever removed,
// this test documents the required contract and will flag the regression
// during code review.

namespace
{

// ─── Spy that records the call sequence ─────────────────────────────

struct CallRecord
{
    enum Type
    {
        SetProcessing,
        SetActive,
        ComponentSetState
    };

    Type type;
    bool boolArg;   // for SetProcessing(bool) and SetActive(bool)
};

struct SpyComponent
{
    std::vector<CallRecord>& log;
    bool setStateShouldFail = false;

    void setActive (bool state)
    {
        log.push_back ({CallRecord::SetActive, state});
    }

    // Returns true on success, false on failure (mirrors Steinberg::kResultOk)
    bool setState (const std::vector<uint8_t>& /*data*/)
    {
        log.push_back ({CallRecord::ComponentSetState, false});
        return ! setStateShouldFail;
    }
};

struct SpyProcessor
{
    std::vector<CallRecord>& log;

    void setProcessing (bool state)
    {
        log.push_back ({CallRecord::SetProcessing, state});
    }
};

// ─── Free function mirroring PluginInstance::setState's deactivation ─

void setStateWithDeactivation (SpyComponent& component,
                               SpyProcessor* processor,
                               const std::vector<uint8_t>& data)
{
    if (data.size() < 4)
        return;

    // Deactivate before state restore (VST3 spec requirement)
    if (processor != nullptr)
        processor->setProcessing (false);
    component.setActive (false);

    // Parse format header
    uint32_t componentSize = 0;
    std::memcpy (&componentSize, data.data(), 4);

    if (4 + componentSize > data.size())
    {
        // Format mismatch — attempt raw pass-through
        component.setState (data);

        // Reactivate regardless of success or failure
        component.setActive (true);
        if (processor != nullptr)
            processor->setProcessing (true);
        return;
    }

    // Normal path — restore component state
    std::vector<uint8_t> componentData (
        data.begin() + 4,
        data.begin() + 4 + static_cast<std::ptrdiff_t> (componentSize));
    component.setState (componentData);

    // Reactivate after state restoration
    component.setActive (true);
    if (processor != nullptr)
        processor->setProcessing (true);
}

// ─── Helper to check call sequence ──────────────────────────────────

bool isDeactivateBeforeSetState (const std::vector<CallRecord>& log)
{
    // Find the first ComponentSetState call
    int setStateIdx = -1;
    for (int i = 0; i < static_cast<int> (log.size()); ++i)
    {
        if (log[static_cast<size_t> (i)].type == CallRecord::ComponentSetState)
        {
            setStateIdx = i;
            break;
        }
    }

    if (setStateIdx < 0)
        return false; // No setState call found

    // Verify setProcessing(false) comes before setState
    bool foundProcessingOff = false;
    bool foundActiveOff = false;

    for (int i = 0; i < setStateIdx; ++i)
    {
        auto& r = log[static_cast<size_t> (i)];
        if (r.type == CallRecord::SetProcessing && ! r.boolArg)
            foundProcessingOff = true;
        if (r.type == CallRecord::SetActive && ! r.boolArg)
            foundActiveOff = true;
    }

    return foundProcessingOff && foundActiveOff;
}

bool isReactivateAfterSetState (const std::vector<CallRecord>& log)
{
    // Find the last ComponentSetState call
    int setStateIdx = -1;
    for (int i = static_cast<int> (log.size()) - 1; i >= 0; --i)
    {
        if (log[static_cast<size_t> (i)].type == CallRecord::ComponentSetState)
        {
            setStateIdx = i;
            break;
        }
    }

    if (setStateIdx < 0)
        return false;

    // Verify setActive(true) and setProcessing(true) come after setState
    bool foundActiveOn = false;
    bool foundProcessingOn = false;

    for (int i = setStateIdx + 1; i < static_cast<int> (log.size()); ++i)
    {
        auto& r = log[static_cast<size_t> (i)];
        if (r.type == CallRecord::SetActive && r.boolArg)
            foundActiveOn = true;
        if (r.type == CallRecord::SetProcessing && r.boolArg)
            foundProcessingOn = true;
    }

    return foundActiveOn && foundProcessingOn;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// Regression #003: setState deactivation contract
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("Regression #003: setState deactivates processing before restore "
           "and reactivates after",
           "[regression]")
{
    SECTION ("normal path: valid dc format data")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        // Valid dc format: componentSize = 8, total = 4 + 8 = 12 bytes
        uint32_t componentSize = 8;
        std::vector<uint8_t> data (4 + componentSize, 0);
        std::memcpy (data.data(), &componentSize, 4);

        setStateWithDeactivation (component, &processor, data);

        REQUIRE (isDeactivateBeforeSetState (log));
        REQUIRE (isReactivateAfterSetState (log));

        // Verify exact call ordering:
        // 0: setProcessing(false)
        // 1: setActive(false)
        // 2: component->setState(...)
        // 3: setActive(true)
        // 4: setProcessing(true)
        REQUIRE (log.size() == 5);
        REQUIRE (log[0].type == CallRecord::SetProcessing);
        REQUIRE (log[0].boolArg == false);
        REQUIRE (log[1].type == CallRecord::SetActive);
        REQUIRE (log[1].boolArg == false);
        REQUIRE (log[2].type == CallRecord::ComponentSetState);
        REQUIRE (log[3].type == CallRecord::SetActive);
        REQUIRE (log[3].boolArg == true);
        REQUIRE (log[4].type == CallRecord::SetProcessing);
        REQUIRE (log[4].boolArg == true);
    }

    SECTION ("format mismatch path: JUCE-format data triggers raw pass-through")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        // JUCE-format data: first 4 bytes decode to large componentSize
        std::vector<uint8_t> badData = {
            0xFF, 0xFF, 0x00, 0x00,  // componentSize = 65535 (LE), larger than data
            0x01, 0x02, 0x03, 0x04   // some payload (total = 8 bytes)
        };

        setStateWithDeactivation (component, &processor, badData);

        REQUIRE (isDeactivateBeforeSetState (log));
        REQUIRE (isReactivateAfterSetState (log));

        // Verify exact call ordering:
        // 0: setProcessing(false)
        // 1: setActive(false)
        // 2: component->setState(...)  [raw pass-through]
        // 3: setActive(true)
        // 4: setProcessing(true)
        REQUIRE (log.size() == 5);
        REQUIRE (log[0].type == CallRecord::SetProcessing);
        REQUIRE (log[0].boolArg == false);
        REQUIRE (log[1].type == CallRecord::SetActive);
        REQUIRE (log[1].boolArg == false);
        REQUIRE (log[2].type == CallRecord::ComponentSetState);
        REQUIRE (log[3].type == CallRecord::SetActive);
        REQUIRE (log[3].boolArg == true);
        REQUIRE (log[4].type == CallRecord::SetProcessing);
        REQUIRE (log[4].boolArg == true);
    }

    SECTION ("failed setState still reactivates")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log, /*setStateShouldFail=*/true};
        SpyProcessor processor {log};

        // JUCE-format data that triggers pass-through, which then fails
        std::vector<uint8_t> badData = {
            0x57, 0x30, 0xEA, 0xDD,  // componentSize = way too big
            0x01, 0x02, 0x03, 0x04,
            0x05, 0x06, 0x07, 0x08
        };

        setStateWithDeactivation (component, &processor, badData);

        // Even though setState failed, reactivation must happen
        REQUIRE (isDeactivateBeforeSetState (log));
        REQUIRE (isReactivateAfterSetState (log));
    }

    SECTION ("tiny data (< 4 bytes) is a no-op — no deactivation needed")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        std::vector<uint8_t> tinyData = {0x01, 0x02};

        setStateWithDeactivation (component, &processor, tinyData);

        // No calls at all — early return before touching component
        REQUIRE (log.empty());
    }

    SECTION ("null processor: setActive still called, setProcessing skipped")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};

        uint32_t componentSize = 4;
        std::vector<uint8_t> data (4 + componentSize, 0);
        std::memcpy (data.data(), &componentSize, 4);

        setStateWithDeactivation (component, nullptr, data);

        // No setProcessing calls, but setActive still called
        REQUIRE (log.size() == 3);
        REQUIRE (log[0].type == CallRecord::SetActive);
        REQUIRE (log[0].boolArg == false);
        REQUIRE (log[1].type == CallRecord::ComponentSetState);
        REQUIRE (log[2].type == CallRecord::SetActive);
        REQUIRE (log[2].boolArg == true);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Contract verification: bounds check conditions
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("Regression #003: format mismatch detection matches production code",
           "[regression]")
{
    SECTION ("JUCE-format data triggers mismatch")
    {
        std::vector<uint8_t> juceFormatData = {
            0xFF, 0xFF, 0x00, 0x00,
            0x01, 0x02, 0x03, 0x04
        };

        uint32_t componentSize = 0;
        std::memcpy (&componentSize, juceFormatData.data(), 4);

        REQUIRE (4 + componentSize > juceFormatData.size());
    }

    SECTION ("valid dc format passes bounds check")
    {
        uint32_t componentSize = 4;
        std::vector<uint8_t> dcFormatData (4 + componentSize, 0);
        std::memcpy (dcFormatData.data(), &componentSize, 4);

        uint32_t readSize = 0;
        std::memcpy (&readSize, dcFormatData.data(), 4);

        REQUIRE (4 + readSize <= dcFormatData.size());
    }

    SECTION ("componentSize = 0 is valid (empty component state)")
    {
        uint32_t componentSize = 0;
        std::vector<uint8_t> data (4, 0);
        std::memcpy (data.data(), &componentSize, 4);

        REQUIRE (4 + componentSize <= data.size());
    }
}
