#include "MeterTapProcessor.h"
#include "dc/foundation/types.h"
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
    if (muted.load())
    {
        audio.clear();
        peakLeft.store (0.0f);
        peakRight.store (0.0f);
        return;
    }

    // Apply gain and pan (post-insert)
    float currentGain = gain.load();
    float currentPan  = pan.load();

    float angle   = currentPan * dc::pi<float> * 0.25f + dc::pi<float> * 0.25f;
    float leftAmp  = currentGain * std::cos (angle);
    float rightAmp = currentGain * std::sin (angle);

    int numChannels = audio.getNumChannels();

    if (numChannels >= 1)
    {
        float* data = audio.getChannel (0);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            data[i] *= leftAmp;
            mag = std::max (mag, std::abs (data[i]));
        }
        float old = peakLeft.load();
        peakLeft.store (std::max (mag, old * 0.95f));
    }

    if (numChannels >= 2)
    {
        float* data = audio.getChannel (1);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            data[i] *= rightAmp;
            mag = std::max (mag, std::abs (data[i]));
        }
        float old = peakRight.load();
        peakRight.store (std::max (mag, old * 0.95f));
    }
}

} // namespace dc
