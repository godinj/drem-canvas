#pragma once
#include <JuceHeader.h>
#include "TransportController.h"
#include <atomic>
#include <array>

namespace dc
{

class StepSequencerProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxRows  = 16;
    static constexpr int maxSteps = 64;

    struct StepData
    {
        bool   active      = false;
        int    velocity    = 100;
        double probability = 1.0;
        double noteLength  = 1.0;   // fraction of one step duration
    };

    struct RowData
    {
        int    noteNumber = 36;
        bool   mute       = false;
        bool   solo       = false;
        std::array<StepData, maxSteps> steps;
    };

    struct PatternSnapshot
    {
        int numRows        = 0;
        int numSteps       = 16;
        int stepDivision   = 4;     // steps per beat
        double swing       = 0.0;
        bool hasSoloedRow  = false;
        std::array<RowData, maxRows> rows;
    };

    explicit StepSequencerProcessor (TransportController& transport);

    // AudioProcessor overrides
    const juce::String getName() const override { return "StepSequencer"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // Lock-free pattern update (called from message thread)
    void updatePatternSnapshot (const PatternSnapshot& snapshot);

    // Tempo (called from message thread)
    void setTempo (double bpm) { tempo.store (bpm); }

    // Current step for GUI playback cursor
    int getCurrentStep() const { return currentStep.load(); }

private:
    TransportController& transportController;

    // Double-buffered pattern data
    PatternSnapshot snapshots[2];
    std::atomic<int> readIndex  { 0 };
    std::atomic<int> writeIndex { 1 };
    std::atomic<bool> newDataReady { false };

    std::atomic<double> tempo { 120.0 };
    std::atomic<int> currentStep { -1 };

    double currentSampleRate = 44100.0;
    double previousStepPosition = 0.0;

    // Note-off tracking
    struct PendingNoteOff
    {
        int    noteNumber = 0;
        int    channel    = 10;  // MIDI channel 10 for drums
        int64_t offSample = 0;
    };

    static constexpr int maxPendingNoteOffs = 128;
    std::array<PendingNoteOff, maxPendingNoteOffs> pendingNoteOffs;
    int numPendingNoteOffs = 0;

    void addNoteOff (int noteNumber, int channel, int64_t offSample);
    void processNoteOffs (juce::MidiBuffer& midiMessages, int64_t blockStart, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerProcessor)
};

} // namespace dc
