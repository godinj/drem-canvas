#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/worker_thread.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("WorkerThread task runs on background thread", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-worker");
    std::atomic<std::thread::id> taskThreadId{};
    std::atomic<bool> done{false};

    worker.submit([&] {
        taskThreadId.store(std::this_thread::get_id());
        done.store(true, std::memory_order_release);
    });

    // Wait for task to complete
    while (!done.load(std::memory_order_acquire))
        std::this_thread::yield();

    REQUIRE(taskThreadId.load() != std::this_thread::get_id());
    worker.stop();
}

TEST_CASE("WorkerThread tasks execute in submission order", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-order");
    std::mutex mu;
    std::string order;
    std::atomic<int> completed{0};

    constexpr int numTasks = 20;

    for (int i = 0; i < numTasks; ++i)
    {
        worker.submit([&, i] {
            std::lock_guard<std::mutex> lock(mu);
            order += std::to_string(i) + ",";
            completed.fetch_add(1, std::memory_order_release);
        });
    }

    // Wait for all tasks
    while (completed.load(std::memory_order_acquire) < numTasks)
        std::this_thread::yield();

    // Build expected order string
    std::string expected;
    for (int i = 0; i < numTasks; ++i)
        expected += std::to_string(i) + ",";

    REQUIRE(order == expected);
    worker.stop();
}

TEST_CASE("WorkerThread stop waits for current task to complete", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-stop-wait");
    std::atomic<bool> taskStarted{false};
    std::atomic<bool> taskFinished{false};

    worker.submit([&] {
        taskStarted.store(true, std::memory_order_release);
        // Simulate a task that takes some time
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        taskFinished.store(true, std::memory_order_release);
    });

    // Wait for task to start
    while (!taskStarted.load(std::memory_order_acquire))
        std::this_thread::yield();

    // stop() should block until the task finishes
    worker.stop();
    REQUIRE(taskFinished.load());
}

TEST_CASE("WorkerThread stop is idempotent", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-idempotent");

    worker.stop();
    worker.stop();  // second call should not crash
    worker.stop();  // third call also fine

    REQUIRE_FALSE(worker.isRunning());
}

TEST_CASE("WorkerThread submit after stop is silently ignored", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-submit-after-stop");
    worker.stop();

    std::atomic<bool> called{false};
    worker.submit([&] { called.store(true); });

    // Give a moment for any spurious execution
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE_FALSE(called.load());
}

TEST_CASE("WorkerThread isRunning state transitions", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-running");
    REQUIRE(worker.isRunning());

    worker.stop();
    REQUIRE_FALSE(worker.isRunning());
}

TEST_CASE("WorkerThread destructor calls stop automatically", "[foundation][worker_thread]")
{
    std::atomic<bool> taskRan{false};

    {
        dc::WorkerThread worker("test-dtor");
        worker.submit([&] {
            taskRan.store(true, std::memory_order_release);
        });
        // Worker goes out of scope here — destructor should call stop()
    }

    // The task may or may not have run depending on timing,
    // but the destructor should not hang or crash.
    // This test verifies no crash/hang on destruction.
    REQUIRE(true);
}

TEST_CASE("WorkerThread handles many submitted tasks", "[foundation][worker_thread]")
{
    dc::WorkerThread worker("test-many");
    std::atomic<int> counter{0};
    constexpr int numTasks = 1000;

    for (int i = 0; i < numTasks; ++i)
    {
        worker.submit([&] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    worker.stop();  // waits for all queued tasks to drain
    REQUIRE(counter.load() == numTasks);
}
