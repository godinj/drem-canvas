#pragma once
#include <string>

namespace dc {

class AudioBlock;
class MidiBlock;

class AudioNode
{
public:
    virtual ~AudioNode() = default;

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void release() {}
    virtual void process (AudioBlock& audio, MidiBlock& midi,
                          int numSamples) = 0;

    virtual int getLatencySamples() const { return 0; }
    virtual int getNumInputChannels() const { return 2; }
    virtual int getNumOutputChannels() const { return 2; }
    virtual bool acceptsMidi() const { return false; }
    virtual bool producesMidi() const { return false; }
    virtual std::string getName() const { return "AudioNode"; }
};

} // namespace dc
