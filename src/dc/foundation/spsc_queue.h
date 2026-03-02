#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>

namespace dc {

/// Lock-free single-producer single-consumer ring buffer.
/// Capacity is rounded up to the next power of two.
template<typename T>
class SPSCQueue
{
public:
    explicit SPSCQueue(size_t capacity)
        : capacity_(nextPowerOf2(capacity))
        , mask_(capacity_ - 1)
        , buffer_(new T[capacity_])
    {
    }

    /// Push an item (producer thread only). Returns false if full.
    bool push(const T& item)
    {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto next = (tail + 1) & mask_;
        if (next == head_.load(std::memory_order_acquire))
            return false;
        buffer_[tail] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    /// Push an item by move (producer thread only). Returns false if full.
    bool push(T&& item)
    {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto next = (tail + 1) & mask_;
        if (next == head_.load(std::memory_order_acquire))
            return false;
        buffer_[tail] = std::move(item);
        tail_.store(next, std::memory_order_release);
        return true;
    }

    /// Pop an item (consumer thread only). Returns false if empty.
    bool pop(T& item)
    {
        auto head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire))
            return false;
        item = std::move(buffer_[head]);
        head_.store((head + 1) & mask_, std::memory_order_release);
        return true;
    }

    size_t size() const
    {
        auto tail = tail_.load(std::memory_order_acquire);
        auto head = head_.load(std::memory_order_acquire);
        return (tail - head) & mask_;
    }

    bool empty() const
    {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    size_t capacity() const { return capacity_ - 1; }  // usable slots

private:
    static size_t nextPowerOf2(size_t n)
    {
        if (n == 0) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }

    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<T[]> buffer_;

    // Separate cache lines to prevent false sharing
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

} // namespace dc
