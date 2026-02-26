#pragma once
#include <JuceHeader.h>
#include "TransportController.h"
#include <atomic>

namespace dc
{

class MetronomeProcessor : public juce::AudioProcessor
{
public:
    MetronomeProcessor (TransportController& transport);

    const juce::String getName() const override { return "Metronome"; }
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

    void setEnabled (bool e) { enabled.store (e); }
    bool isEnabled() const   { return enabled.load(); }
    void setTempo (double bpm) { tempo.store (bpm); }
    void setVolume (float v) { volume.store (v); }
    void setBeatsPerBar (int beats) { beatsPerBar.store (beats); }

private:
    TransportController& transportController;

    std::atomic<bool> enabled { false };
    std::atomic<double> tempo { 120.0 };
    std::atomic<float> volume { 0.7f };
    std::atomic<int> beatsPerBar { 4 };

    double currentSampleRate = 44100.0;
    int clickSampleLength = 0;
    int clickSamplePos = 0;
    double clickFrequency = 1000.0;     // Hz for downbeat
    double clickFrequencyOff = 800.0;   // Hz for other beats
    bool isDownbeat = true;

    double previousBeatPosition = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetronomeProcessor)
};

} // namespace dc
