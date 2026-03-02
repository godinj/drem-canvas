#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <chrono>

// PluginManager depends on the full VST3Host chain which is too heavyweight
// for the unit test target.  We mirror the async scan state machine here
// and verify the state transitions and callback semantics in isolation.

namespace
{

enum class ScanState { idle, scanning, complete };

struct AsyncScanStateMachine
{
    std::atomic<ScanState> scanState { ScanState::idle };

    ScanState getScanState() const { return scanState.load(); }

    /// Simulates scanForPlugins() (synchronous)
    void scanSync()
    {
        scanState = ScanState::scanning;
        // simulate work
        scanState = ScanState::complete;
    }

    /// Simulates scanForPluginsAsync()
    void scanAsync (
        std::function<void (const std::string& name, int current, int total)> onProgress,
        std::function<void ()> onComplete)
    {
        if (scanState == ScanState::scanning)
            return;  // reject concurrent

        scanState = ScanState::scanning;

        std::thread ([this, onProgress = std::move (onProgress),
                      onComplete = std::move (onComplete)] ()
        {
            // Simulate scanning 3 plugins
            for (int i = 1; i <= 3; ++i)
            {
                if (onProgress)
                    onProgress ("Plugin" + std::to_string (i), i, 3);
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }

            scanState = ScanState::complete;

            if (onComplete)
                onComplete();
        }).detach();
    }
};

} // namespace

TEST_CASE ("Async scan state transitions", "[plugins]")
{
    AsyncScanStateMachine sm;
    REQUIRE (sm.getScanState() == ScanState::idle);

    SECTION ("synchronous scan transitions through scanning to complete")
    {
        sm.scanSync();
        REQUIRE (sm.getScanState() == ScanState::complete);
    }

    SECTION ("async scan sets state to scanning then complete")
    {
        std::atomic<bool> done { false };

        sm.scanAsync (
            nullptr,
            [&done] () { done = true; });

        // State should be scanning (or already complete if very fast)
        auto state = sm.getScanState();
        REQUIRE ((state == ScanState::scanning
                  || state == ScanState::complete));

        // Wait for completion (max 5 seconds)
        for (int i = 0; i < 500 && ! done; ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));

        REQUIRE (done);
        REQUIRE (sm.getScanState() == ScanState::complete);
    }

    SECTION ("async scan rejects concurrent scan request")
    {
        std::atomic<bool> done { false };
        std::atomic<int> completionCount { 0 };

        sm.scanAsync (
            nullptr,
            [&done, &completionCount] ()
            {
                ++completionCount;
                done = true;
            });

        // Try to start a second scan while first is running
        std::atomic<bool> secondDone { false };
        sm.scanAsync (
            nullptr,
            [&secondDone] () { secondDone = true; });

        // Wait for first scan to complete
        for (int i = 0; i < 500 && ! done; ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));

        REQUIRE (done);

        // The second scan should not have been triggered
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
        REQUIRE (completionCount == 1);
    }

    SECTION ("progress callback receives correct values")
    {
        std::atomic<bool> done { false };
        std::atomic<int> lastCurrent { 0 };
        std::atomic<int> lastTotal { 0 };

        sm.scanAsync (
            [&lastCurrent, &lastTotal] (const std::string&, int current, int total)
            {
                lastCurrent = current;
                lastTotal = total;
            },
            [&done] () { done = true; });

        for (int i = 0; i < 500 && ! done; ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));

        REQUIRE (done);
        REQUIRE (lastCurrent == 3);
        REQUIRE (lastTotal == 3);
    }
}
