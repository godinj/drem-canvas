#include "MeterTapProcessor.h"
#include <algorithm>
#include <cmath>

namespace dc
{

MeterTapProcessor::MeterTapProcessor()
{
}

void MeterTapProcessor::prepare (double /*sampleRate*/, int /*maxBlockSize*/)
{
    peakLeft.store (0.0f);
    peakRight.store (0.0f);
}

void MeterTapProcessor::release()
{
}

void MeterTapProcessor::process (AudioBlock& audio, MidiBlock& /*midi*/, int numSamples)
{
    // Pass audio through unchanged — just measure peaks
    if (audio.getNumChannels() >= 1)
    {
        const float* data = audio.getChannel (0);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float old = peakLeft.load();
        peakLeft.store (std::max (mag, old * 0.95f));
    }

    if (audio.getNumChannels() >= 2)
    {
        const float* data = audio.getChannel (1);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float old = peakRight.load();
        peakRight.store (std::max (mag, old * 0.95f));
    }
}

} // namespace dc
