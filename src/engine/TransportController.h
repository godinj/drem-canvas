#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace dc
{

class TransportController
{
public:
    TransportController();

    void play();
    void stop();
    void togglePlayStop();

    bool isPlaying() const { return playing.load(); }

    // Position in samples
    int64_t getPositionInSamples() const { return positionInSamples.load(); }
    void setPositionInSamples (int64_t newPos) { positionInSamples.store (newPos); }

    // Called from audio thread
    void advancePosition (int numSamples);

    double getSampleRate() const { return sampleRate.load(); }
    void setSampleRate (double sr) { sampleRate.store (sr); }

    // Time display helpers
    double getPositionInSeconds() const;
    juce::String getTimeString() const;

private:
    std::atomic<bool> playing { false };
    std::atomic<int64_t> positionInSamples { 0 };
    std::atomic<double> sampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportController)
};

} // namespace dc
