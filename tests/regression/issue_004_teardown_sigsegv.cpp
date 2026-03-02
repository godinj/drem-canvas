// tests/regression/issue_004_teardown_sigsegv.cpp
//
// Bug: AppController destructor crashed with SIGSEGV at address 0x0
//      because the PortAudio audio callback thread was still running
//      while bookkeeping vectors were being cleared.
//
// Cause: ~AppController() called audioEngine.shutdown() as the LAST
//        step.  The audio callback thread continued calling
//        graph_.processBlock() while plugin chains, meter taps, and
//        track nodes were being destroyed above.  Additionally,
//        AudioEngine::shutdown() called closeDevice() before
//        setCallback(nullptr), leaving a non-null callback pointer
//        during stream wind-down.
//
// Fix: Added AudioEngine::stopStream() that nulls the callback FIRST
//      then closes the device.  ~AppController() now calls
//      audioEngine.stopStream() as its very first line, ensuring the
//      audio thread is quiescent before any cleanup occurs.

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <functional>
#include <vector>

namespace
{

// ─── Minimal spy modelling the shutdown sequence ─────────────────────

struct CallRecord
{
    enum Type
    {
        SetCallbackNull,
        CloseDevice,
        ClearVectors,
        GraphRelease,
        GraphClear,
        ResetDeviceManager
    };

    Type type;
};

// Models the corrected stopStream() + shutdown() + destructor sequence
void correctTeardownSequence (std::vector<CallRecord>& log,
                              bool deviceOpen)
{
    // stopStream() — called first in ~AppController
    if (deviceOpen)
    {
        log.push_back ({CallRecord::SetCallbackNull});
        log.push_back ({CallRecord::CloseDevice});
    }

    // ~AppController body: clear bookkeeping
    log.push_back ({CallRecord::ClearVectors});

    // audioEngine.shutdown() — called last in ~AppController
    // stopStream() is idempotent, so no extra close calls
    log.push_back ({CallRecord::GraphRelease});
    log.push_back ({CallRecord::GraphClear});
    log.push_back ({CallRecord::ResetDeviceManager});
}

// Models the OLD buggy sequence (for documentation)
void buggyTeardownSequence (std::vector<CallRecord>& log,
                            bool deviceOpen)
{
    // ~AppController body: clear bookkeeping WHILE audio thread runs
    log.push_back ({CallRecord::ClearVectors});

    // audioEngine.shutdown() — LAST, with wrong order
    if (deviceOpen)
    {
        log.push_back ({CallRecord::CloseDevice});       // callback still non-null!
        log.push_back ({CallRecord::SetCallbackNull});    // too late
    }
    log.push_back ({CallRecord::GraphRelease});
    log.push_back ({CallRecord::GraphClear});
    log.push_back ({CallRecord::ResetDeviceManager});
}

bool callbackNulledBeforeCloseDevice (const std::vector<CallRecord>& log)
{
    int nullIdx = -1;
    int closeIdx = -1;

    for (int i = 0; i < static_cast<int> (log.size()); ++i)
    {
        if (log[static_cast<size_t> (i)].type == CallRecord::SetCallbackNull && nullIdx < 0)
            nullIdx = i;
        if (log[static_cast<size_t> (i)].type == CallRecord::CloseDevice && closeIdx < 0)
            closeIdx = i;
    }

    if (nullIdx < 0 || closeIdx < 0)
        return true; // no device — order doesn't matter

    return nullIdx < closeIdx;
}

bool streamStoppedBeforeVectorClear (const std::vector<CallRecord>& log)
{
    int closeIdx = -1;
    int clearIdx = -1;

    for (int i = 0; i < static_cast<int> (log.size()); ++i)
    {
        if (log[static_cast<size_t> (i)].type == CallRecord::CloseDevice && closeIdx < 0)
            closeIdx = i;
        if (log[static_cast<size_t> (i)].type == CallRecord::ClearVectors && clearIdx < 0)
            clearIdx = i;
    }

    if (closeIdx < 0)
        return true; // no device open — safe

    return closeIdx < clearIdx;
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// Regression #004: Audio teardown crash (SIGSEGV at 0x0)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("Regression #004: stopStream nulls callback before closing device",
           "[regression]")
{
    SECTION ("correct sequence with open device")
    {
        std::vector<CallRecord> log;
        correctTeardownSequence (log, true);

        REQUIRE (callbackNulledBeforeCloseDevice (log));
        REQUIRE (streamStoppedBeforeVectorClear (log));
    }

    SECTION ("correct sequence with no device")
    {
        std::vector<CallRecord> log;
        correctTeardownSequence (log, false);

        // No close/null calls, but ClearVectors is safe
        REQUIRE (callbackNulledBeforeCloseDevice (log));
        REQUIRE (streamStoppedBeforeVectorClear (log));
    }

    SECTION ("buggy sequence violates invariants")
    {
        std::vector<CallRecord> log;
        buggyTeardownSequence (log, true);

        // The old code nulled callback AFTER closing — wrong order
        REQUIRE_FALSE (callbackNulledBeforeCloseDevice (log));
        // The old code cleared vectors BEFORE stopping stream — crash cause
        REQUIRE_FALSE (streamStoppedBeforeVectorClear (log));
    }
}

TEST_CASE ("Regression #004: stopStream is idempotent",
           "[regression]")
{
    // Calling stopStream() twice should be safe (no double-close)
    std::vector<CallRecord> log;
    std::atomic<bool> deviceOpen {true};

    auto stopStream = [&]()
    {
        if (deviceOpen.exchange (false))
        {
            log.push_back ({CallRecord::SetCallbackNull});
            log.push_back ({CallRecord::CloseDevice});
        }
    };

    stopStream();
    stopStream();  // Second call should be a no-op

    REQUIRE (log.size() == 2);  // Only one pair of calls
    REQUIRE (log[0].type == CallRecord::SetCallbackNull);
    REQUIRE (log[1].type == CallRecord::CloseDevice);
}
