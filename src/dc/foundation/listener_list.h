#pragma once

#include <algorithm>
#include <vector>

namespace dc {

template<typename ListenerType>
class ListenerList
{
public:
    void add(ListenerType* listener)
    {
        if (listener != nullptr &&
            std::find(listeners_.begin(), listeners_.end(), listener) == listeners_.end())
        {
            listeners_.push_back(listener);
        }
    }

    void remove(ListenerType* listener)
    {
        listeners_.erase(
            std::remove(listeners_.begin(), listeners_.end(), listener),
            listeners_.end());
    }

    /// Call a method on all listeners.
    /// Iterates a copy so listeners may remove themselves during the callback.
    template<typename Callback>
    void call(Callback&& callback)
    {
        auto copy = listeners_;
        for (auto* l : copy)
            if (l) callback(*l);
    }

    int size() const { return static_cast<int>(listeners_.size()); }
    bool isEmpty() const { return listeners_.empty(); }

private:
    std::vector<ListenerType*> listeners_;
};

} // namespace dc
