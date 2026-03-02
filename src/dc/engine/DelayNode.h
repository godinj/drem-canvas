#pragma once

#include "dc/engine/AudioNode.h"

#include <vector>

namespace dc {

/// Plugin Delay Compensation node. Inserted by the graph when parallel
/// branches have different latencies.
class DelayNode : public AudioNode
{
public:
    void setDelay (int samples);
    int getDelay() const { return delaySamples_; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override;
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;
    std::string getName() const override { return "DelayNode"; }

private:
    int delaySamples_ = 0;
    int writePos_ = 0;
    int maxBlockSize_ = 0;
    double sampleRate_ = 44100.0;
    bool prepared_ = false;
    std::vector<std::vector<float>> delayLines_;  // per-channel circular buffer

    void allocateDelayLines();
};

} // namespace dc
