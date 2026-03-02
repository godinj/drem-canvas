#pragma once

// Adapter: wraps a dc::PluginInstance as a dc::AudioNode.
// Since dc::PluginInstance already implements AudioNode, this is a thin
// ownership wrapper that allows the audio graph to own the plugin instance
// while callers can still access the plugin pointer.

#include "dc/plugins/PluginInstance.h"
#include "dc/engine/AudioNode.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"
#include <memory>
#include <string>

namespace dc
{

class PluginProcessorNode : public AudioNode
{
public:
    explicit PluginProcessorNode (std::unique_ptr<dc::PluginInstance> instance)
        : plugin_ (std::move (instance))
    {
    }

    dc::PluginInstance* getPlugin() { return plugin_.get(); }

    // AudioNode interface — delegate to PluginInstance
    void prepare (double sampleRate, int maxBlockSize) override
    {
        plugin_->prepare (sampleRate, maxBlockSize);
    }

    void release() override
    {
        plugin_->release();
    }

    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override
    {
        plugin_->process (audio, midi, numSamples);
    }

    int getLatencySamples() const override
    {
        return plugin_->getLatencySamples();
    }

    int getNumInputChannels() const override
    {
        return plugin_->getNumInputChannels();
    }

    int getNumOutputChannels() const override
    {
        return plugin_->getNumOutputChannels();
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
        return plugin_->getName();
    }

private:
    std::unique_ptr<dc::PluginInstance> plugin_;

    PluginProcessorNode (const PluginProcessorNode&) = delete;
    PluginProcessorNode& operator= (const PluginProcessorNode&) = delete;
};

} // namespace dc
