#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/audio/AudioBlock.h"
#include <atomic>

namespace dc
{

class MetronomeProcessor : public AudioNode
{
public:
    MetronomeProcessor (TransportController& transport);

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override;
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "Metronome"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }

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

    MetronomeProcessor (const MetronomeProcessor&) = delete;
    MetronomeProcessor& operator= (const MetronomeProcessor&) = delete;
};

} // namespace dc
