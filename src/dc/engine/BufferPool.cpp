#include "dc/engine/BufferPool.h"
#include "dc/foundation/assert.h"

#include <cstring>

namespace dc {

void BufferPool::prepare (int numBuffers, int numChannels, int maxBlockSize)
{
    maxChannels_ = numChannels;
    maxBlockSize_ = maxBlockSize;

    buffers_.resize (static_cast<size_t> (numBuffers));

    for (auto& buf : buffers_)
    {
        buf.storage.resize (static_cast<size_t> (numChannels * maxBlockSize), 0.0f);
        buf.channelPtrs.resize (static_cast<size_t> (numChannels));

        for (int ch = 0; ch < numChannels; ++ch)
            buf.channelPtrs[static_cast<size_t> (ch)] = buf.storage.data() + ch * maxBlockSize;

        buf.inUse.store (false, std::memory_order_relaxed);
    }
}

AudioBlock BufferPool::acquire (int numChannels, int numSamples)
{
    for (auto& buf : buffers_)
    {
        bool expected = false;

        if (buf.inUse.compare_exchange_strong (expected, true, std::memory_order_relaxed))
        {
            // Zero the requested region
            int ch = std::min (numChannels, maxChannels_);

            for (int c = 0; c < ch; ++c)
                std::memset (buf.channelPtrs[static_cast<size_t> (c)], 0,
                             sizeof (float) * static_cast<size_t> (numSamples));

            return AudioBlock (buf.channelPtrs.data(), ch, numSamples);
        }
    }

    // Pool exhausted — this is a graph topology bug, not a runtime condition
    dc_assert (false && "BufferPool exhausted");
    return AudioBlock();
}

void BufferPool::release (AudioBlock block)
{
    if (block.getNumChannels() == 0)
        return;

    float* targetPtr = block.getChannel (0);

    for (auto& buf : buffers_)
    {
        if (! buf.channelPtrs.empty() && buf.channelPtrs[0] == targetPtr)
        {
            buf.inUse.store (false, std::memory_order_relaxed);
            return;
        }
    }
}

void BufferPool::releaseAll()
{
    for (auto& buf : buffers_)
        buf.inUse.store (false, std::memory_order_relaxed);
}

} // namespace dc
