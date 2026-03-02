#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/audio/AudioBlock.h"
#include <atomic>

namespace dc
{

class MixBusProcessor : public AudioNode
{
public:
    MixBusProcessor (TransportController& transport);

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override;
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "MixBus"; }
    int getNumInputChannels() const override { return 2; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }

    // Metering - read from GUI thread
    float getPeakLevelLeft() const  { return peakLeft.load(); }
    float getPeakLevelRight() const { return peakRight.load(); }
    void resetPeaks() { peakLeft.store (0.0f); peakRight.store (0.0f); }

    // Master gain
    void setMasterGain (float g) { masterGain.store (g); }
    float getMasterGain() const  { return masterGain.load(); }

private:
    TransportController& transportController;
    std::atomic<float> peakLeft { 0.0f };
    std::atomic<float> peakRight { 0.0f };
    std::atomic<float> masterGain { 1.0f };

    MixBusProcessor (const MixBusProcessor&) = delete;
    MixBusProcessor& operator= (const MixBusProcessor&) = delete;
};

} // namespace dc
