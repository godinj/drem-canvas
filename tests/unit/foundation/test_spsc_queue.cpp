#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/spsc_queue.h>
#include <atomic>
#include <memory>
#include <thread>

TEST_CASE("SpscQueue push/pop FIFO ordering", "[foundation][spsc_queue]")
{
    dc::SPSCQueue<int> q(8);
    REQUIRE(q.push(1));
    REQUIRE(q.push(2));
    REQUIRE(q.push(3));

    int val = 0;
    REQUIRE(q.pop(val));
    REQUIRE(val == 1);
    REQUIRE(q.pop(val));
    REQUIRE(val == 2);
    REQUIRE(q.pop(val));
    REQUIRE(val == 3);
}

TEST_CASE("SpscQueue push to full queue returns false", "[foundation][spsc_queue]")
{
    dc::SPSCQueue<int> q(4);

    // Fill the queue to capacity
    size_t pushed = 0;
    while (q.push(static_cast<int>(pushed)))
        ++pushed;

    // Capacity is usable slots = nextPowerOf2(4) - 1 = 3
    REQUIRE(pushed == q.capacity());

    // Next push should fail
    REQUIRE_FALSE(q.push(999));
}

TEST_CASE("SpscQueue pop from empty queue returns false", "[foundation][spsc_queue]")
{
    dc::SPSCQueue<int> q(8);
    int val = 42;
    REQUIRE_FALSE(q.pop(val));
    REQUIRE(val == 42);  // unchanged
}

TEST_CASE("SpscQueue capacity rounds to power of 2", "[foundation][spsc_queue]")
{
    // Requesting capacity 5 should round up to 8 internally,
    // and usable capacity is 8-1=7
    dc::SPSCQueue<int> q(5);
    REQUIRE(q.capacity() >= 5);

    // Verify it is a power-of-2 minus 1 (i.e., capacity + 1 is power of 2)
    size_t cap = q.capacity();
    size_t internal = cap + 1;
    REQUIRE((internal & (internal - 1)) == 0);
}

TEST_CASE("SpscQueue size and empty", "[foundation][spsc_queue]")
{
    dc::SPSCQueue<int> q(8);
    REQUIRE(q.empty());
    REQUIRE(q.size() == 0);

    q.push(1);
    REQUIRE_FALSE(q.empty());
    REQUIRE(q.size() == 1);

    q.push(2);
    REQUIRE(q.size() == 2);

    int val;
    q.pop(val);
    REQUIRE(q.size() == 1);

    q.pop(val);
    REQUIRE(q.empty());
    REQUIRE(q.size() == 0);
}

TEST_CASE("SpscQueue wrap-around", "[foundation][spsc_queue]")
{
    dc::SPSCQueue<int> q(4);
    size_t cap = q.capacity();

    // Fill, drain, fill again to force wrap-around
    for (size_t i = 0; i < cap; ++i)
        REQUIRE(q.push(static_cast<int>(i)));

    int val;
    for (size_t i = 0; i < cap; ++i)
    {
        REQUIRE(q.pop(val));
        REQUIRE(val == static_cast<int>(i));
    }

    // Now push again — indices have wrapped
    for (size_t i = 0; i < cap; ++i)
        REQUIRE(q.push(static_cast<int>(i + 100)));

    for (size_t i = 0; i < cap; ++i)
    {
        REQUIRE(q.pop(val));
        REQUIRE(val == static_cast<int>(i + 100));
    }

    REQUIRE(q.empty());
}

TEST_CASE("SpscQueue multi-threaded stress test", "[foundation][spsc_queue]")
{
    constexpr int numItems = 100000;
    dc::SPSCQueue<int> q(1024);

    std::atomic<bool> producerDone{false};
    int consumedCount = 0;
    int lastValue = -1;
    bool orderCorrect = true;

    // Producer thread
    std::thread producer([&] {
        for (int i = 0; i < numItems; ++i)
        {
            while (!q.push(i))
            {
                // Spin until space available
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&] {
        while (true)
        {
            int val;
            if (q.pop(val))
            {
                if (val != lastValue + 1)
                    orderCorrect = false;
                lastValue = val;
                ++consumedCount;
                if (consumedCount == numItems)
                    break;
            }
            else if (producerDone.load(std::memory_order_acquire) && q.empty())
            {
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(consumedCount == numItems);
    REQUIRE(orderCorrect);
    REQUIRE(lastValue == numItems - 1);
}

TEST_CASE("SpscQueue move-only types", "[foundation][spsc_queue]")
{
    dc::SPSCQueue<std::unique_ptr<int>> q(8);

    REQUIRE(q.push(std::make_unique<int>(42)));
    REQUIRE(q.push(std::make_unique<int>(99)));

    std::unique_ptr<int> val;
    REQUIRE(q.pop(val));
    REQUIRE(*val == 42);
    REQUIRE(q.pop(val));
    REQUIRE(*val == 99);
}

TEST_CASE("SpscQueue with capacity 1", "[foundation][spsc_queue]")
{
    // capacity(1) rounds to nextPowerOf2(1) = 1, usable = 0
    // Actually nextPowerOf2(1): n=1, n--=0, all shifts on 0, n+1=1
    // So internal capacity = 1, mask = 0, usable = 0
    // This is a degenerate case, but it should not crash
    dc::SPSCQueue<int> q(1);
    // Usable capacity is 0 — can't push anything
    // This documents the edge case behaviour
    REQUIRE(q.capacity() == 0);
}
