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
    void returnToZero() { positionInSamples.store (0); }

    // Called from audio thread
    void advancePosition (int numSamples);

    double getSampleRate() const { return sampleRate.load(); }
    void setSampleRate (double sr) { sampleRate.store (sr); }

    // Time display helpers
    double getPositionInSeconds() const;
    juce::String getTimeString() const;

    // Loop control
    bool isLooping() const { return loopEnabled.load(); }
    void setLoopEnabled (bool enabled) { loopEnabled.store (enabled); }
    int64_t getLoopStartInSamples() const { return loopStartInSamples.load(); }
    void setLoopStartInSamples (int64_t pos) { loopStartInSamples.store (pos); }
    int64_t getLoopEndInSamples() const { return loopEndInSamples.load(); }
    void setLoopEndInSamples (int64_t pos) { loopEndInSamples.store (pos); }

    // Record arm
    bool isRecordArmed() const { return recordArmed.load(); }
    void setRecordArmed (bool armed) { recordArmed.store (armed); }
    void toggleRecordArm() { recordArmed.store (! recordArmed.load()); }

private:
    std::atomic<bool> playing { false };
    std::atomic<int64_t> positionInSamples { 0 };
    std::atomic<double> sampleRate { 44100.0 };

    // Loop state
    std::atomic<bool> loopEnabled { false };
    std::atomic<int64_t> loopStartInSamples { 0 };
    std::atomic<int64_t> loopEndInSamples { INT64_MAX };

    // Record state
    std::atomic<bool> recordArmed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportController)
};

} // namespace dc
