#pragma once
#include <JuceHeader.h>
#include "TransportController.h"
#include <atomic>
#include <array>

namespace dc
{

class MidiClipProcessor : public juce::AudioProcessor
{
public:
    struct MidiNoteEvent
    {
        int noteNumber;
        int channel;
        int velocity;
        int64_t onSample;   // absolute sample position on timeline
        int64_t offSample;  // absolute sample position on timeline
    };

    struct MidiTrackSnapshot
    {
        int numEvents = 0;
        static constexpr int maxEvents = 4096;
        std::array<MidiNoteEvent, maxEvents> events;  // pre-sorted by onSample
    };

    explicit MidiClipProcessor (TransportController& transport);

    // AudioProcessor overrides
    const juce::String getName() const override { return "MidiClipProcessor"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override  { return true; }
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

    // Lock-free snapshot update (called from message thread)
    void updateSnapshot (const MidiTrackSnapshot& snapshot);

    // Inject a live MIDI message from the message thread (lock-free SPSC FIFO)
    void injectLiveMidi (const juce::MidiMessage& msg);

    // Tempo (called from message thread)
    void setTempo (double bpm) { tempo.store (bpm); }

    // Gain/pan/mute for mixing (mirrors TrackProcessor interface)
    void setGain (float g)   { gain.store (g); }
    void setPan (float p)    { pan.store (p); }
    void setMuted (bool m)   { muted.store (m); }

    float getPeakLevelLeft() const  { return peakLeft.load(); }
    float getPeakLevelRight() const { return peakRight.load(); }

private:
    TransportController& transportController;

    // Double-buffered snapshot data
    MidiTrackSnapshot snapshots[2];
    std::atomic<int> readIndex  { 0 };
    std::atomic<int> writeIndex { 1 };
    std::atomic<bool> newDataReady { false };

    std::atomic<double> tempo { 120.0 };
    double currentSampleRate = 44100.0;

    // Gain/pan/mute
    std::atomic<float> gain { 1.0f };
    std::atomic<float> pan  { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<float> peakLeft  { 0.0f };
    std::atomic<float> peakRight { 0.0f };

    // Live MIDI injection FIFO (SPSC: message thread → audio thread)
    static constexpr int liveMidiFifoSize = 256;
    juce::AbstractFifo liveMidiFifo { liveMidiFifoSize };
    std::array<juce::MidiMessage, liveMidiFifoSize> liveMidiBuffer;

    void drainLiveMidiFifo (juce::MidiBuffer& midiMessages);

    // Note-off tracking
    struct PendingNoteOff
    {
        int    noteNumber = 0;
        int    channel    = 1;
        int64_t offSample = 0;
    };

    static constexpr int maxPendingNoteOffs = 256;
    std::array<PendingNoteOff, maxPendingNoteOffs> pendingNoteOffs;
    int numPendingNoteOffs = 0;

    void addNoteOff (int noteNumber, int channel, int64_t offSample);
    void processNoteOffs (juce::MidiBuffer& midiMessages, int64_t blockStart, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClipProcessor)
};

} // namespace dc
