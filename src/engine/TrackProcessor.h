#pragma once
#include <JuceHeader.h>
#include "TransportController.h"

namespace dc
{

class TrackProcessor : public juce::AudioProcessor
{
public:
    TrackProcessor (TransportController& transport);
    ~TrackProcessor() override;

    bool loadFile (const juce::File& file);
    void clearFile();

    // AudioProcessor interface
    const juce::String getName() const override { return "TrackProcessor"; }
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

    // Gain/pan
    void setGain (float g) { gain.store (g); }
    float getGain() const  { return gain.load(); }
    void setPan (float p)  { pan.store (p); }
    float getPan() const   { return pan.load(); }
    void setMuted (bool m) { muted.store (m); }
    bool isMuted() const   { return muted.load(); }

    int64_t getFileLengthInSamples() const;

    // Metering
    float getPeakLevelLeft() const  { return peakLeft.load(); }
    float getPeakLevelRight() const { return peakRight.load(); }

private:
    TransportController& transportController;
    juce::AudioFormatManager formatManager;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    std::atomic<float> gain { 1.0f };
    std::atomic<float> pan { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<float> peakLeft { 0.0f };
    std::atomic<float> peakRight { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackProcessor)
};

} // namespace dc
