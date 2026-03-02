#pragma once

#include <cstring>

namespace dc {

/// Non-owning view over float** channel data.
/// Lightweight wrapper for passing multi-channel audio between components.
class AudioBlock
{
public:
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

    void clear()
    {
        for (int ch = 0; ch < numChannels_; ++ch)
            std::memset (channels_[ch], 0, sizeof (float) * static_cast<size_t> (numSamples_));
    }

private:
    float** channels_;
    int numChannels_;
    int numSamples_;
};

} // namespace dc
