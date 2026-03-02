#include "dc/foundation/message_queue.h"

namespace dc {

void MessageQueue::post(std::function<void()> fn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(fn));
}

void MessageQueue::processAll()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        processing_.swap(queue_);
    }

    for (auto& fn : processing_)
        fn();

    processing_.clear();
}

size_t MessageQueue::pending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace dc
