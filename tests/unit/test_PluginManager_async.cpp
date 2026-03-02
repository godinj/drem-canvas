#include <catch2/catch_test_macros.hpp>
#include "plugins/PluginManager.h"
#include "dc/foundation/message_queue.h"
#include <atomic>
#include <chrono>
#include <thread>

TEST_CASE ("PluginManager async scan", "[integration]")
{
    dc::MessageQueue messageQueue;
    dc::PluginManager pm (messageQueue);

    SECTION ("isScanning returns false initially")
    {
        REQUIRE (pm.isScanning() == false);
    }

    SECTION ("scanForPluginsAsync sets isScanning to true")
    {
        pm.scanForPluginsAsync();
        REQUIRE (pm.isScanning() == true);

        // Wait for completion so destructor doesn't race
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (120);
        while (pm.isScanning() && std::chrono::steady_clock::now() < deadline)
        {
            messageQueue.processAll();
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
        }
    }

    SECTION ("async scan completes and calls onComplete")
    {
        std::atomic<bool> completeCalled { false };
        std::atomic<int> progressCount { 0 };
        std::atomic<int> lastTotal { 0 };

        pm.scanForPluginsAsync (
            [&progressCount, &lastTotal] (const std::string& /*name*/, int /*current*/, int total)
            {
                progressCount.fetch_add (1);
                lastTotal.store (total);
            },
            [&completeCalled]()
            {
                completeCalled.store (true);
            }
        );

        REQUIRE (pm.isScanning() == true);

        // Pump message queue until scan completes (up to 120s for yabridge plugins)
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (120);
        while (! completeCalled.load() && std::chrono::steady_clock::now() < deadline)
        {
            messageQueue.processAll();
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
        }

        REQUIRE (completeCalled.load() == true);
        REQUIRE (pm.isScanning() == false);

        // If plugins were found, verify progress fired
        int total = lastTotal.load();
        if (total > 0)
        {
            CHECK (progressCount.load() > 0);
        }
    }

    SECTION ("scanForPluginsAsync is no-op while scanning")
    {
        std::atomic<int> completeCount { 0 };

        pm.scanForPluginsAsync ({}, [&completeCount]() { completeCount.fetch_add (1); });

        // Second call should be a no-op
        pm.scanForPluginsAsync ({}, [&completeCount]() { completeCount.fetch_add (1); });

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (120);
        while (pm.isScanning() && std::chrono::steady_clock::now() < deadline)
        {
            messageQueue.processAll();
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
        }

        // Pump a bit more for any straggling messages
        for (int i = 0; i < 10; ++i)
        {
            messageQueue.processAll();
            std::this_thread::sleep_for (std::chrono::milliseconds (20));
        }

        REQUIRE (completeCount.load() == 1);
    }
}
