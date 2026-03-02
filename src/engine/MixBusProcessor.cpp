#include "MixBusProcessor.h"
#include <algorithm>
#include <cmath>

namespace dc
{

MixBusProcessor::MixBusProcessor (TransportController& transport)
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      transportController (transport)
{
}

void MixBusProcessor::prepareToPlay (double /*sampleRate*/, int /*maximumExpectedSamplesPerBlock*/)
{
    resetPeaks();
}

void MixBusProcessor::releaseResources()
{
}

void MixBusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    dc::AudioBlock block (buffer.getArrayOfWritePointers(),
                          buffer.getNumChannels(), buffer.getNumSamples());

    transportController.advancePosition (block.getNumSamples());

    const float gain = masterGain.load();
    const int numSamples = block.getNumSamples();

    // Apply master gain to all channels
    for (int ch = 0; ch < block.getNumChannels(); ++ch)
    {
        float* data = block.getChannel (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= gain;
    }

    // Calculate peak levels for left and right channels
    if (block.getNumChannels() >= 1)
    {
        const float* data = block.getChannel (0);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float oldPeak = peakLeft.load();
        peakLeft.store (std::max (mag, oldPeak * 0.95f));
    }

    if (block.getNumChannels() >= 2)
    {
        const float* data = block.getChannel (1);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float oldPeak = peakRight.load();
        peakRight.store (std::max (mag, oldPeak * 0.95f));
    }
}

} // namespace dc
