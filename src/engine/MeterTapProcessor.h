#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "dc/audio/AudioBlock.h"
#include <atomic>

namespace dc
{

/**
 * Transparent pass-through processor that measures peak audio levels.
 * Inserted at the end of each track's plugin chain (before MixBus)
 * to provide post-insert metering for both audio and MIDI tracks.
 */
class MeterTapProcessor : public AudioNode
{
public:
    MeterTapProcessor();

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override;
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "MeterTap"; }
    int getNumInputChannels() const override { return 2; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }

    // Metering — read from GUI thread
    float getPeakLevelLeft() const  { return peakLeft.load(); }
    float getPeakLevelRight() const { return peakRight.load(); }

private:
    std::atomic<float> peakLeft { 0.0f };
    std::atomic<float> peakRight { 0.0f };

    MeterTapProcessor (const MeterTapProcessor&) = delete;
    MeterTapProcessor& operator= (const MeterTapProcessor&) = delete;
};

} // namespace dc
