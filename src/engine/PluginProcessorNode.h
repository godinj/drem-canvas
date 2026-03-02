#pragma once

// Adapter: wraps a juce::AudioPluginInstance as a dc::AudioNode.
// This bridge exists until Phase 4 (plugin hosting migration) replaces
// juce::AudioPluginInstance with a native dc:: plugin host.

#include <JuceHeader.h>
#include "dc/engine/AudioNode.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"
#include "dc/midi/MidiBuffer.h"
#include <memory>
#include <string>
#include <cstring>

namespace dc
{

class PluginProcessorNode : public AudioNode
{
public:
    explicit PluginProcessorNode (std::unique_ptr<juce::AudioPluginInstance> instance)
        : plugin_ (std::move (instance))
    {
    }

    juce::AudioPluginInstance* getPlugin() { return plugin_.get(); }

    // AudioNode interface
    void prepare (double sampleRate, int maxBlockSize) override
    {
        plugin_->setPlayConfigDetails (
            plugin_->getTotalNumInputChannels(),
            plugin_->getTotalNumOutputChannels(),
            sampleRate, maxBlockSize);
        plugin_->prepareToPlay (sampleRate, maxBlockSize);

        // Pre-allocate juce buffers for process()
        juceBuffer_.setSize (std::max (plugin_->getTotalNumInputChannels(),
                                        plugin_->getTotalNumOutputChannels()),
                             maxBlockSize, false, true, false);
    }

    void release() override
    {
        plugin_->releaseResources();
    }

    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override
    {
        int numCh = audio.getNumChannels();
        juceBuffer_.setSize (numCh, numSamples, true, false, true);

        // Copy dc::AudioBlock into juce::AudioBuffer
        for (int ch = 0; ch < numCh; ++ch)
            std::memcpy (juceBuffer_.getWritePointer (ch), audio.getChannel (ch),
                         sizeof (float) * static_cast<size_t> (numSamples));

        // Convert dc::MidiBlock to juce::MidiBuffer
        juceMidi_.clear();
        for (auto it = midi.begin(); it != midi.end(); ++it)
        {
            auto event = *it;
            juceMidi_.addEvent (juce::MidiMessage (event.message.getRawData(),
                                                    event.message.getRawDataSize()),
                                event.sampleOffset);
        }

        plugin_->processBlock (juceBuffer_, juceMidi_);

        // Copy back to dc::AudioBlock
        for (int ch = 0; ch < numCh; ++ch)
            std::memcpy (audio.getChannel (ch), juceBuffer_.getReadPointer (ch),
                         sizeof (float) * static_cast<size_t> (numSamples));

        // Convert MIDI output back to dc::MidiBlock
        midi.clear();
        for (const auto metadata : juceMidi_)
        {
            midi.addEvent (dc::MidiMessage (metadata.data, metadata.numBytes),
                           metadata.samplePosition);
        }
    }

    int getLatencySamples() const override
    {
        return plugin_->getLatencySamples();
    }

    int getNumInputChannels() const override
    {
        return plugin_->getTotalNumInputChannels();
    }

    int getNumOutputChannels() const override
    {
        return plugin_->getTotalNumOutputChannels();
    }

    bool acceptsMidi() const override
    {
        return plugin_->acceptsMidi();
    }

    bool producesMidi() const override
    {
        return plugin_->producesMidi();
    }

    std::string getName() const override
    {
        return plugin_->getName().toStdString();
    }

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin_;
    juce::AudioBuffer<float> juceBuffer_;
    juce::MidiBuffer juceMidi_;

    PluginProcessorNode (const PluginProcessorNode&) = delete;
    PluginProcessorNode& operator= (const PluginProcessorNode&) = delete;
};

} // namespace dc
