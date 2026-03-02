#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace dc {

class WorkerThread
{
public:
    explicit WorkerThread(const std::string& name);
    ~WorkerThread();

    /// Submit a task to the worker thread
    void submit(std::function<void()> task);

    /// Stop the thread (waits for current task to finish)
    void stop();

    bool isRunning() const;

private:
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::atomic<bool> running_{true};
    std::string name_;

    void run();
};

} // namespace dc
