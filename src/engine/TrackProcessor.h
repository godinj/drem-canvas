#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/audio/DiskStreamer.h"
#include <filesystem>
#include <memory>
#include <atomic>

namespace dc
{

class TrackProcessor : public AudioNode
{
public:
    TrackProcessor (TransportController& transport);
    ~TrackProcessor() override;

    bool loadFile (const std::filesystem::path& file);
    void clearFile();

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override;
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "TrackProcessor"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }

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
    std::unique_ptr<dc::DiskStreamer> diskStreamer;

    std::atomic<float> gain { 1.0f };
    std::atomic<float> pan { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<float> peakLeft { 0.0f };
    std::atomic<float> peakRight { 0.0f };

    int64_t lastSeekPosition = -1;

    TrackProcessor (const TrackProcessor&) = delete;
    TrackProcessor& operator= (const TrackProcessor&) = delete;
};

} // namespace dc
