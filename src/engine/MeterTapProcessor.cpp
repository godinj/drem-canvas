#include "MeterTapProcessor.h"

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
    // Pass audio through unchanged — just measure peaks
    if (buffer.getNumChannels() >= 1)
    {
        float mag = buffer.getMagnitude (0, 0, buffer.getNumSamples());
        float old = peakLeft.load();
        peakLeft.store (std::max (mag, old * 0.95f));
    }

    if (buffer.getNumChannels() >= 2)
    {
        float mag = buffer.getMagnitude (1, 0, buffer.getNumSamples());
        float old = peakRight.load();
        peakRight.store (std::max (mag, old * 0.95f));
    }
}

} // namespace dc
