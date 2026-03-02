#pragma once

#include <string>

namespace dc {

class AudioBlock;
class MidiBlock;

/// Pure virtual interface replacing juce::AudioProcessor.
/// Deliberately simpler — no parameter system, no editor, no bus layout negotiation.
class AudioNode
{
public:
    virtual ~AudioNode() = default;

    /// Called once when the graph is prepared (sample rate, block size known)
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;

    /// Called once when the graph is released
    virtual void release() {}

    /// Process one block of audio and MIDI.
    /// audio: interleaved channel buffers (read/write)
    /// midi: timestamped MIDI events for this block (read/write)
    /// numSamples: number of samples to process (<= maxBlockSize)
    virtual void process (AudioBlock& audio, MidiBlock& midi, int numSamples) = 0;

    /// Report latency in samples (for PDC). Default: 0.
    virtual int getLatencySamples() const { return 0; }

    /// Number of input/output audio channels
    virtual int getNumInputChannels() const { return 2; }
    virtual int getNumOutputChannels() const { return 2; }

    /// Whether this node accepts/produces MIDI
    virtual bool acceptsMidi() const { return false; }
    virtual bool producesMidi() const { return false; }

    /// Human-readable name (for debugging/display)
    virtual std::string getName() const { return "AudioNode"; }
};

} // namespace dc
