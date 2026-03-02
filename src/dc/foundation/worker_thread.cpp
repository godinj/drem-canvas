#include "dc/foundation/worker_thread.h"

namespace dc {

WorkerThread::WorkerThread(const std::string& name)
    : name_(name)
{
    thread_ = std::thread([this] { run(); });
}

WorkerThread::~WorkerThread()
{
    stop();
}

void WorkerThread::submit(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void WorkerThread::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }
    cv_.notify_one();

    if (thread_.joinable())
        thread_.join();
}

bool WorkerThread::isRunning() const
{
    return running_.load(std::memory_order_relaxed);
}

void WorkerThread::run()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !running_ || !tasks_.empty(); });

            if (!running_ && tasks_.empty())
                return;

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

} // namespace dc
