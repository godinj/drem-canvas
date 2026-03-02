// tests/regression/issue_005_set_state_setup_processing.cpp
//
// Bug: PluginInstance::process() SIGSEGV inside yabridge after setState()
//      during session restore. Phase Plant crashed because setState()
//      called setActive(true) without first calling setupProcessing().
//
// Cause: VST3 lifecycle requires setupProcessing() before setActive(true)
//        after a deactivation cycle. yabridge-bridged plugins enforce this
//        strictly; native plugins tolerate the omission.
//
// Fix: setState() now calls setupProcessing() before reactivation in both
//      the format-mismatch and normal state restoration paths.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// PluginInstance is not easily instantiated in tests (requires a loaded
// VST3 module).  Instead, we model the setupProcessing contract with a
// lightweight spy and a free function that mirrors PluginInstance::setState's
// lifecycle logic.  This extends the pattern from issue_003.

namespace
{

// ─── Spy that records the call sequence ─────────────────────────────

struct CallRecord
{
    enum Type
    {
        SetProcessing,
        SetActive,
        ComponentSetState,
        SetupProcessing
    };

    Type type;
    bool boolArg;       // for SetProcessing(bool) and SetActive(bool)
    double sampleRate;  // for SetupProcessing
    int blockSize;      // for SetupProcessing
};

struct SpyComponent
{
    std::vector<CallRecord>& log;
    bool setStateShouldFail = false;

    void setActive (bool state)
    {
        log.push_back ({CallRecord::SetActive, state, 0.0, 0});
    }

    // Returns true on success, false on failure (mirrors Steinberg::kResultOk)
    bool setState (const std::vector<uint8_t>& /*data*/)
    {
        log.push_back ({CallRecord::ComponentSetState, false, 0.0, 0});
        return ! setStateShouldFail;
    }
};

struct SpyProcessor
{
    std::vector<CallRecord>& log;

    void setProcessing (bool state)
    {
        log.push_back ({CallRecord::SetProcessing, state, 0.0, 0});
    }

    // Mirrors PluginInstance::setupProcessing which calls
    // processor_->setupProcessing(setup), component_->setActive(true),
    // processor_->setProcessing(true)
    void setupProcessing (SpyComponent& component, double sampleRate, int blockSize)
    {
        log.push_back ({CallRecord::SetupProcessing, false, sampleRate, blockSize});
        component.setActive (true);
        setProcessing (true);
    }
};

// ─── Free function mirroring PluginInstance::setState's lifecycle ───

void setStateWithSetupProcessing (SpyComponent& component,
                                  SpyProcessor* processor,
                                  const std::vector<uint8_t>& data,
                                  double currentSampleRate,
                                  int currentBlockSize)
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

        // Reactivate with full VST3 lifecycle (setupProcessing before setActive)
        if (processor != nullptr)
            processor->setupProcessing (component, currentSampleRate, currentBlockSize);
        else
            component.setActive (true);
        return;
    }

    // Normal path — restore component state
    std::vector<uint8_t> componentData (
        data.begin() + 4,
        data.begin() + 4 + static_cast<std::ptrdiff_t> (componentSize));
    component.setState (componentData);

    // Reactivate with full VST3 lifecycle (setupProcessing before setActive)
    if (processor != nullptr)
        processor->setupProcessing (component, currentSampleRate, currentBlockSize);
    else
        component.setActive (true);
}

// ─── Helper: find call index by type ────────────────────────────────

int findCall (const std::vector<CallRecord>& log, CallRecord::Type type, int startFrom = 0)
{
    for (int i = startFrom; i < static_cast<int> (log.size()); ++i)
    {
        if (log[static_cast<size_t> (i)].type == type)
            return i;
    }
    return -1;
}

int findCallWithArg (const std::vector<CallRecord>& log, CallRecord::Type type,
                     bool arg, int startFrom = 0)
{
    for (int i = startFrom; i < static_cast<int> (log.size()); ++i)
    {
        auto& r = log[static_cast<size_t> (i)];
        if (r.type == type && r.boolArg == arg)
            return i;
    }
    return -1;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// Regression #005: setupProcessing called before reactivation in setState
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("Regression #005: setState calls setupProcessing before reactivation",
           "[regression]")
{
    const double testSampleRate = 48000.0;
    const int testBlockSize = 256;

    SECTION ("normal path: setupProcessing appears after setActive(false) and before setActive(true)")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        // Valid dc format: componentSize = 8, total = 4 + 8 = 12 bytes
        uint32_t componentSize = 8;
        std::vector<uint8_t> data (4 + componentSize, 0);
        std::memcpy (data.data(), &componentSize, 4);

        setStateWithSetupProcessing (component, &processor, data,
                                     testSampleRate, testBlockSize);

        // Verify complete call sequence:
        // 0: SetProcessing(false)
        // 1: SetActive(false)
        // 2: ComponentSetState
        // 3: SetupProcessing(48000, 256)
        // 4: SetActive(true)      [called by setupProcessing]
        // 5: SetProcessing(true)  [called by setupProcessing]
        REQUIRE (log.size() == 6);
        REQUIRE (log[0].type == CallRecord::SetProcessing);
        REQUIRE (log[0].boolArg == false);
        REQUIRE (log[1].type == CallRecord::SetActive);
        REQUIRE (log[1].boolArg == false);
        REQUIRE (log[2].type == CallRecord::ComponentSetState);
        REQUIRE (log[3].type == CallRecord::SetupProcessing);
        REQUIRE (log[3].sampleRate == testSampleRate);
        REQUIRE (log[3].blockSize == testBlockSize);
        REQUIRE (log[4].type == CallRecord::SetActive);
        REQUIRE (log[4].boolArg == true);
        REQUIRE (log[5].type == CallRecord::SetProcessing);
        REQUIRE (log[5].boolArg == true);

        // Key invariant: SetupProcessing comes after SetActive(false)
        // and before SetActive(true)
        int deactivateIdx = findCallWithArg (log, CallRecord::SetActive, false);
        int setupIdx = findCall (log, CallRecord::SetupProcessing);
        int reactivateIdx = findCallWithArg (log, CallRecord::SetActive, true);

        REQUIRE (deactivateIdx < setupIdx);
        REQUIRE (setupIdx < reactivateIdx);
    }

    SECTION ("format mismatch path: setupProcessing appears after setActive(false) and before setActive(true)")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        // JUCE-format data: first 4 bytes decode to large componentSize
        std::vector<uint8_t> badData = {
            0xFF, 0xFF, 0x00, 0x00,  // componentSize = 65535 (LE), larger than data
            0x01, 0x02, 0x03, 0x04   // some payload (total = 8 bytes)
        };

        setStateWithSetupProcessing (component, &processor, badData,
                                     testSampleRate, testBlockSize);

        // Verify complete call sequence:
        // 0: SetProcessing(false)
        // 1: SetActive(false)
        // 2: ComponentSetState  [raw pass-through]
        // 3: SetupProcessing(48000, 256)
        // 4: SetActive(true)
        // 5: SetProcessing(true)
        REQUIRE (log.size() == 6);
        REQUIRE (log[0].type == CallRecord::SetProcessing);
        REQUIRE (log[0].boolArg == false);
        REQUIRE (log[1].type == CallRecord::SetActive);
        REQUIRE (log[1].boolArg == false);
        REQUIRE (log[2].type == CallRecord::ComponentSetState);
        REQUIRE (log[3].type == CallRecord::SetupProcessing);
        REQUIRE (log[3].sampleRate == testSampleRate);
        REQUIRE (log[3].blockSize == testBlockSize);
        REQUIRE (log[4].type == CallRecord::SetActive);
        REQUIRE (log[4].boolArg == true);
        REQUIRE (log[5].type == CallRecord::SetProcessing);
        REQUIRE (log[5].boolArg == true);

        // Key invariant
        int deactivateIdx = findCallWithArg (log, CallRecord::SetActive, false);
        int setupIdx = findCall (log, CallRecord::SetupProcessing);
        int reactivateIdx = findCallWithArg (log, CallRecord::SetActive, true);

        REQUIRE (deactivateIdx < setupIdx);
        REQUIRE (setupIdx < reactivateIdx);
    }

    SECTION ("failed setState: setupProcessing is called even when state restore fails")
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

        setStateWithSetupProcessing (component, &processor, badData,
                                     testSampleRate, testBlockSize);

        // Even though setState failed, setupProcessing + reactivation must happen
        int setupIdx = findCall (log, CallRecord::SetupProcessing);
        REQUIRE (setupIdx >= 0);

        int reactivateIdx = findCallWithArg (log, CallRecord::SetActive, true);
        REQUIRE (reactivateIdx >= 0);
        REQUIRE (setupIdx < reactivateIdx);
    }

    SECTION ("complete sequence: SetProcessing(false) -> SetActive(false) -> "
             "ComponentSetState -> SetupProcessing -> SetActive(true) -> SetProcessing(true)")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        uint32_t componentSize = 4;
        std::vector<uint8_t> data (4 + componentSize, 0);
        std::memcpy (data.data(), &componentSize, 4);

        setStateWithSetupProcessing (component, &processor, data,
                                     testSampleRate, testBlockSize);

        // Verify the complete contract:
        // SetProcessing(false) before ComponentSetState
        int processingOffIdx = findCallWithArg (log, CallRecord::SetProcessing, false);
        int setStateIdx = findCall (log, CallRecord::ComponentSetState);
        REQUIRE (processingOffIdx < setStateIdx);

        // SetActive(false) before ComponentSetState
        int activeOffIdx = findCallWithArg (log, CallRecord::SetActive, false);
        REQUIRE (activeOffIdx < setStateIdx);

        // SetupProcessing after ComponentSetState
        int setupIdx = findCall (log, CallRecord::SetupProcessing);
        REQUIRE (setStateIdx < setupIdx);

        // SetActive(true) after SetupProcessing
        int activeOnIdx = findCallWithArg (log, CallRecord::SetActive, true);
        REQUIRE (setupIdx < activeOnIdx);

        // SetProcessing(true) after SetActive(true)
        int processingOnIdx = findCallWithArg (log, CallRecord::SetProcessing, true);
        REQUIRE (activeOnIdx < processingOnIdx);
    }

    SECTION ("tiny data (< 4 bytes) is a no-op — no calls at all")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        std::vector<uint8_t> tinyData = {0x01, 0x02};

        setStateWithSetupProcessing (component, &processor, tinyData,
                                     testSampleRate, testBlockSize);

        REQUIRE (log.empty());
    }

    SECTION ("null processor: setActive still called but no setupProcessing or setProcessing")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};

        uint32_t componentSize = 4;
        std::vector<uint8_t> data (4 + componentSize, 0);
        std::memcpy (data.data(), &componentSize, 4);

        setStateWithSetupProcessing (component, nullptr, data,
                                     testSampleRate, testBlockSize);

        // Without a processor, setupProcessing cannot be called.
        // Only setActive(false), ComponentSetState, setActive(true)
        REQUIRE (log.size() == 3);
        REQUIRE (log[0].type == CallRecord::SetActive);
        REQUIRE (log[0].boolArg == false);
        REQUIRE (log[1].type == CallRecord::ComponentSetState);
        REQUIRE (log[2].type == CallRecord::SetActive);
        REQUIRE (log[2].boolArg == true);

        // No SetupProcessing call
        REQUIRE (findCall (log, CallRecord::SetupProcessing) == -1);
    }

    SECTION ("setupProcessing receives correct sampleRate and blockSize")
    {
        std::vector<CallRecord> log;
        SpyComponent component {log};
        SpyProcessor processor {log};

        const double rate = 96000.0;
        const int blockSz = 1024;

        uint32_t componentSize = 4;
        std::vector<uint8_t> data (4 + componentSize, 0);
        std::memcpy (data.data(), &componentSize, 4);

        setStateWithSetupProcessing (component, &processor, data, rate, blockSz);

        int setupIdx = findCall (log, CallRecord::SetupProcessing);
        REQUIRE (setupIdx >= 0);
        REQUIRE (log[static_cast<size_t> (setupIdx)].sampleRate == rate);
        REQUIRE (log[static_cast<size_t> (setupIdx)].blockSize == blockSz);
    }
}
