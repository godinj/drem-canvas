#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/midi/MidiMessage.h"
#include "dc/midi/MidiBuffer.h"
#include "dc/audio/AudioBlock.h"
#include "dc/foundation/spsc_queue.h"
#include <atomic>
#include <array>

namespace dc
{

class MidiClipProcessor : public AudioNode
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

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override {}
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "MidiClipProcessor"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }

    // Lock-free snapshot update (called from message thread)
    void updateSnapshot (const MidiTrackSnapshot& snapshot);

    // Inject a live MIDI message from the message thread (lock-free SPSC FIFO)
    void injectLiveMidi (const dc::MidiMessage& msg);

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

    // Live MIDI injection FIFO (SPSC: message thread -> audio thread)
    dc::SPSCQueue<dc::MidiMessage> liveMidiFifo { 256 };

    void drainLiveMidiFifo (MidiBlock& midi);

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

    int64_t previousBlockStart = 0;

    void addNoteOff (int noteNumber, int channel, int64_t offSample);
    void processNoteOffs (MidiBlock& midi, int64_t blockStart, int numSamples);

    MidiClipProcessor (const MidiClipProcessor&) = delete;
    MidiClipProcessor& operator= (const MidiClipProcessor&) = delete;
};

} // namespace dc
