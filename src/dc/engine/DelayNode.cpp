#include "dc/engine/DelayNode.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"

namespace dc {

void DelayNode::setDelay (int samples)
{
    delaySamples_ = samples;

    if (prepared_)
        allocateDelayLines();
}

void DelayNode::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    prepared_ = true;
    allocateDelayLines();
}

void DelayNode::release()
{
    delayLines_.clear();
    prepared_ = false;
    writePos_ = 0;
}

void DelayNode::process (AudioBlock& audio, MidiBlock& /*midi*/, int numSamples)
{
    // MIDI: pass through unchanged (MIDI delay compensation is not implemented)

    if (delaySamples_ == 0)
        return;  // pass-through

    int numChannels = audio.getNumChannels();
    int delaySize = delaySamples_;

    for (int ch = 0; ch < numChannels && ch < static_cast<int> (delayLines_.size()); ++ch)
    {
        auto& delayLine = delayLines_[static_cast<size_t> (ch)];
        float* channelData = audio.getChannel (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            int readPos = (writePos_ + i) % delaySize;
            float delayed = delayLine[static_cast<size_t> (readPos)];
            delayLine[static_cast<size_t> (readPos)] = channelData[i];
            channelData[i] = delayed;
        }
    }

    writePos_ = (writePos_ + numSamples) % delaySize;
}

void DelayNode::allocateDelayLines()
{
    int numChannels = getNumOutputChannels();
    int size = (delaySamples_ > 0) ? delaySamples_ : 1;

    delayLines_.resize (static_cast<size_t> (numChannels));

    for (auto& line : delayLines_)
    {
        line.resize (static_cast<size_t> (size), 0.0f);
        std::fill (line.begin(), line.end(), 0.0f);
    }

    writePos_ = 0;
}

} // namespace dc
