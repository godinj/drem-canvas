#include "MeterTapProcessor.h"
#include <algorithm>
#include <cmath>

namespace dc
{

MeterTapProcessor::MeterTapProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MeterTapProcessor::prepareToPlay (double /*sampleRate*/, int /*maximumExpectedSamplesPerBlock*/)
{
    peakLeft.store (0.0f);
    peakRight.store (0.0f);
}

void MeterTapProcessor::releaseResources()
{
}

void MeterTapProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    dc::AudioBlock block (buffer.getArrayOfWritePointers(),
                          buffer.getNumChannels(), buffer.getNumSamples());
    const int numSamples = block.getNumSamples();

    // Pass audio through unchanged — just measure peaks
    if (block.getNumChannels() >= 1)
    {
        const float* data = block.getChannel (0);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float old = peakLeft.load();
        peakLeft.store (std::max (mag, old * 0.95f));
    }

    if (block.getNumChannels() >= 2)
    {
        const float* data = block.getChannel (1);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float old = peakRight.load();
        peakRight.store (std::max (mag, old * 0.95f));
    }
}

} // namespace dc
