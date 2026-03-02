#include "MixBusProcessor.h"
#include <algorithm>
#include <cmath>

namespace dc
{

MixBusProcessor::MixBusProcessor (TransportController& transport)
    : transportController (transport)
{
}

void MixBusProcessor::prepare (double /*sampleRate*/, int /*maxBlockSize*/)
{
    resetPeaks();
}

void MixBusProcessor::release()
{
}

void MixBusProcessor::process (AudioBlock& audio, MidiBlock& /*midi*/, int numSamples)
{
    transportController.advancePosition (numSamples);

    const float gain = masterGain.load();

    // Apply master gain to all channels
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        float* data = audio.getChannel (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= gain;
    }

    // Calculate peak levels for left and right channels
    if (audio.getNumChannels() >= 1)
    {
        const float* data = audio.getChannel (0);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float oldPeak = peakLeft.load();
        peakLeft.store (std::max (mag, oldPeak * 0.95f));
    }

    if (audio.getNumChannels() >= 2)
    {
        const float* data = audio.getChannel (1);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float oldPeak = peakRight.load();
        peakRight.store (std::max (mag, oldPeak * 0.95f));
    }
}

} // namespace dc
