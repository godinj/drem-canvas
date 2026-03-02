#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/audio/AudioBlock.h"
#include "dc/midi/MidiBuffer.h"
#include <atomic>
#include <array>

namespace dc
{

class StepSequencerProcessor : public AudioNode
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

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override {}
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "StepSequencer"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }

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
    void processNoteOffs (MidiBlock& midi, int64_t blockStart, int numSamples);

    StepSequencerProcessor (const StepSequencerProcessor&) = delete;
    StepSequencerProcessor& operator= (const StepSequencerProcessor&) = delete;
};

} // namespace dc
