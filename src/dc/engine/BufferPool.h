#pragma once

#include "dc/audio/AudioBlock.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace dc {

/// Pre-allocated audio buffer pool. Zero allocation on the audio thread.
/// All methods are called from the audio thread only (single-threaded executor).
class BufferPool
{
public:
    /// Prepare the pool with enough buffers for the graph.
    /// Called on the message thread during graph prepare().
    void prepare (int numBuffers, int numChannels, int maxBlockSize);

    /// Acquire a buffer (audio thread, lock-free).
    /// Returns a zeroed AudioBlock. Asserts if pool is exhausted.
    AudioBlock acquire (int numChannels, int numSamples);

    /// Release a specific buffer back to the pool.
    void release (AudioBlock block);

    /// Release all buffers (called at end of processBlock).
    void releaseAll();

private:
    struct Buffer
    {
        std::vector<float> storage;       // flat: numChannels * maxBlockSize
        std::vector<float*> channelPtrs;  // pointers into storage
        std::atomic<bool> inUse{false};

        Buffer() = default;

        Buffer (Buffer&& other) noexcept
            : storage (std::move (other.storage))
            , channelPtrs (std::move (other.channelPtrs))
            , inUse (other.inUse.load (std::memory_order_relaxed))
        {
        }

        Buffer& operator= (Buffer&& other) noexcept
        {
            storage = std::move (other.storage);
            channelPtrs = std::move (other.channelPtrs);
            inUse.store (other.inUse.load (std::memory_order_relaxed),
                         std::memory_order_relaxed);
            return *this;
        }

        Buffer (const Buffer&) = delete;
        Buffer& operator= (const Buffer&) = delete;
    };

    std::vector<Buffer> buffers_;
    int maxChannels_ = 0;
    int maxBlockSize_ = 0;
};

} // namespace dc
