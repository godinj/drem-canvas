#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/message_queue.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("MessageQueue post then processAll fires callback", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    bool called = false;
    mq.post([&] { called = true; });
    REQUIRE_FALSE(called);

    mq.processAll();
    REQUIRE(called);
}

TEST_CASE("MessageQueue FIFO ordering", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    std::string order;

    mq.post([&] { order += "A"; });
    mq.post([&] { order += "B"; });
    mq.post([&] { order += "C"; });

    mq.processAll();
    REQUIRE(order == "ABC");
}

TEST_CASE("MessageQueue pending count", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    REQUIRE(mq.pending() == 0);

    mq.post([] {});
    REQUIRE(mq.pending() == 1);

    mq.post([] {});
    REQUIRE(mq.pending() == 2);

    mq.processAll();
    REQUIRE(mq.pending() == 0);
}

TEST_CASE("MessageQueue callback that posts new callback defers to next processAll", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    std::string order;

    mq.post([&] {
        order += "A";
        // Post a new callback during processing
        mq.post([&] { order += "C"; });
    });
    mq.post([&] { order += "B"; });

    // First processAll: fires A and B; the callback posted by A is deferred
    mq.processAll();
    REQUIRE(order == "AB");

    // Second processAll: fires C
    mq.processAll();
    REQUIRE(order == "ABC");
}

TEST_CASE("MessageQueue processAll on empty queue is no-op", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    // Should not crash
    mq.processAll();
    REQUIRE(mq.pending() == 0);
}

TEST_CASE("MessageQueue multi-thread post safety", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    constexpr int numThreads = 10;
    constexpr int postsPerThread = 100;
    std::atomic<int> counter{0};

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&] {
            for (int i = 0; i < postsPerThread; ++i)
            {
                mq.post([&] { counter.fetch_add(1); });
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    // All posts are queued
    REQUIRE(mq.pending() == numThreads * postsPerThread);

    // Process all on main thread
    mq.processAll();
    REQUIRE(counter.load() == numThreads * postsPerThread);
    REQUIRE(mq.pending() == 0);
}

TEST_CASE("MessageQueue multiple processAll cycles", "[foundation][message_queue]")
{
    dc::MessageQueue mq;
    int counter = 0;

    mq.post([&] { counter += 1; });
    mq.processAll();
    REQUIRE(counter == 1);

    mq.post([&] { counter += 10; });
    mq.post([&] { counter += 100; });
    mq.processAll();
    REQUIRE(counter == 111);
}
