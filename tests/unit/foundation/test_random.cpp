#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/types.h>
#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("randomFloat values are in [0.0, 1.0)", "[foundation][random]")
{
    for (int i = 0; i < 1000; ++i)
    {
        float val = dc::randomFloat();
        REQUIRE(val >= 0.0f);
        REQUIRE(val < 1.0f);
    }
}

TEST_CASE("randomInt values are in [min, max] inclusive", "[foundation][random]")
{
    for (int i = 0; i < 1000; ++i)
    {
        int val = dc::randomInt(10, 20);
        REQUIRE(val >= 10);
        REQUIRE(val <= 20);
    }
}

TEST_CASE("randomInt with min == max always returns that value", "[foundation][random]")
{
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(dc::randomInt(5, 5) == 5);
    }
}

TEST_CASE("randomInt with negative range", "[foundation][random]")
{
    for (int i = 0; i < 1000; ++i)
    {
        int val = dc::randomInt(-10, -5);
        REQUIRE(val >= -10);
        REQUIRE(val <= -5);
    }
}

TEST_CASE("randomFloat thread safety - concurrent calls do not crash", "[foundation][random]")
{
    constexpr int numThreads = 10;
    constexpr int callsPerThread = 1000;
    std::atomic<int> completedThreads{0};
    std::atomic<bool> rangeOk{true};

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&] {
            for (int i = 0; i < callsPerThread; ++i)
            {
                float val = dc::randomFloat();
                if (val < 0.0f || val >= 1.0f)
                    rangeOk.store(false, std::memory_order_relaxed);
            }
            completedThreads.fetch_add(1);
        });
    }

    for (auto& thread : threads)
        thread.join();

    REQUIRE(completedThreads.load() == numThreads);
    REQUIRE(rangeOk.load());
}

TEST_CASE("randomInt thread safety - concurrent calls do not crash", "[foundation][random]")
{
    constexpr int numThreads = 10;
    constexpr int callsPerThread = 1000;
    std::atomic<int> completedThreads{0};
    std::atomic<bool> rangeOk{true};

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&] {
            for (int i = 0; i < callsPerThread; ++i)
            {
                int val = dc::randomInt(0, 100);
                if (val < 0 || val > 100)
                    rangeOk.store(false, std::memory_order_relaxed);
            }
            completedThreads.fetch_add(1);
        });
    }

    for (auto& thread : threads)
        thread.join();

    REQUIRE(completedThreads.load() == numThreads);
    REQUIRE(rangeOk.load());
}
