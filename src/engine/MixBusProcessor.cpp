#include "MixBusProcessor.h"

namespace dc
{

MixBusProcessor::MixBusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
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
    const float gain = masterGain.load();

    // Apply master gain to all channels
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.applyGain (ch, 0, buffer.getNumSamples(), gain);

    // Calculate peak levels for left and right channels
    if (buffer.getNumChannels() >= 1)
    {
        const float magnitudeLeft = buffer.getMagnitude (0, 0, buffer.getNumSamples());
        float oldPeak = peakLeft.load();
        float newPeak = std::max (magnitudeLeft, oldPeak * 0.95f);
        peakLeft.store (newPeak);
    }

    if (buffer.getNumChannels() >= 2)
    {
        const float magnitudeRight = buffer.getMagnitude (1, 0, buffer.getNumSamples());
        float oldPeak = peakRight.load();
        float newPeak = std::max (magnitudeRight, oldPeak * 0.95f);
        peakRight.store (newPeak);
    }
}

} // namespace dc
