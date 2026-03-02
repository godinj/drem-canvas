#pragma once

#include <algorithm>
#include <cstring>

namespace dc {

/// Non-owning view over float** channel data.
/// Lightweight wrapper for passing multi-channel audio between components.
class AudioBlock
{
public:
    AudioBlock() = default;

    AudioBlock (float** channelData, int numChannels, int numSamples)
        : channels_ (channelData)
        , numChannels_ (numChannels)
        , numSamples_ (numSamples)
    {
    }

    /// Overload for juce::AudioBuffer::getArrayOfWritePointers() which
    /// returns float* const* (pointer to const array of channel pointers).
    AudioBlock (float* const* channelData, int numChannels, int numSamples)
        : channels_ (const_cast<float**> (channelData))
        , numChannels_ (numChannels)
        , numSamples_ (numSamples)
    {
    }

    float* getChannel (int ch) { return channels_[ch]; }
    const float* getChannel (int ch) const { return channels_[ch]; }

    int getNumChannels() const { return numChannels_; }
    int getNumSamples() const { return numSamples_; }

    /// Zero all channels, all samples
    void clear()
    {
        for (int ch = 0; ch < numChannels_; ++ch)
            std::memset (channels_[ch], 0, sizeof (float) * static_cast<size_t> (numSamples_));
    }

    /// Zero a range of samples on all channels
    void clear (int startSample, int numSamples)
    {
        for (int ch = 0; ch < numChannels_; ++ch)
            std::memset (channels_[ch] + startSample, 0, sizeof (float) * static_cast<size_t> (numSamples));
    }

    /// Add all channels from source (mix). Processes min of both channel/sample counts.
    void addFrom (const AudioBlock& source)
    {
        int ch = std::min (numChannels_, source.numChannels_);
        int ns = std::min (numSamples_, source.numSamples_);

        for (int c = 0; c < ch; ++c)
        {
            float* dst = channels_[c];
            const float* src = source.channels_[c];

            for (int i = 0; i < ns; ++i)
                dst[i] += src[i];
        }
    }

    /// Add one channel from source with gain
    void addFrom (int destChannel, const AudioBlock& source, int sourceChannel,
                  int numSamples, float gain = 1.0f)
    {
        float* dst = channels_[destChannel];
        const float* src = source.channels_[sourceChannel];

        if (gain == 1.0f)
        {
            for (int i = 0; i < numSamples; ++i)
                dst[i] += src[i];
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                dst[i] += src[i] * gain;
        }
    }

    /// Copy all channels from source. Processes min of both channel/sample counts.
    void copyFrom (const AudioBlock& source)
    {
        int ch = std::min (numChannels_, source.numChannels_);
        int ns = std::min (numSamples_, source.numSamples_);

        for (int c = 0; c < ch; ++c)
            std::memcpy (channels_[c], source.channels_[c], sizeof (float) * static_cast<size_t> (ns));
    }

    /// Copy one channel from source
    void copyFrom (int destChannel, const AudioBlock& source,
                   int sourceChannel, int numSamples)
    {
        std::memcpy (channels_[destChannel], source.channels_[sourceChannel],
                     sizeof (float) * static_cast<size_t> (numSamples));
    }

    /// Apply gain to all channels, all samples
    void applyGain (float gain)
    {
        for (int ch = 0; ch < numChannels_; ++ch)
        {
            float* data = channels_[ch];

            for (int i = 0; i < numSamples_; ++i)
                data[i] *= gain;
        }
    }

    /// Apply gain to a range on one channel
    void applyGain (int channel, int startSample, int numSamples, float gain)
    {
        float* data = channels_[channel] + startSample;

        for (int i = 0; i < numSamples; ++i)
            data[i] *= gain;
    }

    /// Return a view offset into existing buffers (zero-alloc sub-block)
    AudioBlock getSubBlock (int startSample, int numSamples) const
    {
        // Build offset pointer array — use a small static thread_local buffer
        // to avoid allocation. Max 32 channels.
        static thread_local float* offsetPtrs[32];
        int ch = std::min (numChannels_, 32);

        for (int i = 0; i < ch; ++i)
            offsetPtrs[i] = channels_[i] + startSample;

        return AudioBlock (offsetPtrs, ch, numSamples);
    }

private:
    float** channels_ = nullptr;
    int numChannels_ = 0;
    int numSamples_ = 0;
};

} // namespace dc
