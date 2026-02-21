#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "TransportController.h"

namespace dc
{

class MixBusProcessor : public juce::AudioProcessor
{
public:
    MixBusProcessor (TransportController& transport);

    const juce::String getName() const override { return "MixBus"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixBusProcessor)
};

} // namespace dc
