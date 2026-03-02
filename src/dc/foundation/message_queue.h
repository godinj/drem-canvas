#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace dc {

class MessageQueue
{
public:
    /// Post a callback to be executed on the message thread
    void post(std::function<void()> fn);

    /// Process all pending callbacks (called from message thread event loop)
    void processAll();

    /// Number of pending callbacks
    size_t pending() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::function<void()>> queue_;
    std::vector<std::function<void()>> processing_;  // swap buffer
};

} // namespace dc
