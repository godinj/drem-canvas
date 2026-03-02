#pragma once
#include <JuceHeader.h>
#include "dc/audio/AudioDeviceManager.h"
#include <memory>
#include <string>

namespace dc
{

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    void initialise (int numInputChannels, int numOutputChannels);
    void shutdown();

    juce::AudioProcessorGraph& getGraph()            { return *graph; }

    // Node management
    juce::AudioProcessorGraph::Node::Ptr addProcessor (std::unique_ptr<juce::AudioProcessor> processor);
    void removeProcessor (juce::AudioProcessorGraph::NodeID nodeId);
    void connectNodes (juce::AudioProcessorGraph::NodeID source, int sourceChannel,
                       juce::AudioProcessorGraph::NodeID dest, int destChannel);

    juce::AudioProcessorGraph::Node::Ptr getAudioInputNode()  const { return audioInputNode; }
    juce::AudioProcessorGraph::Node::Ptr getAudioOutputNode() const { return audioOutputNode; }

    // Device info accessors (replace getDeviceManager())
    double getSampleRate() const;
    int getBufferSize() const;
    std::string getCurrentDeviceName() const;

private:
    class GraphCallback;

    std::unique_ptr<dc::AudioDeviceManager> deviceManager_;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    std::unique_ptr<GraphCallback> graphCallback_;

    juce::AudioProcessorGraph::Node::Ptr audioInputNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::AudioProcessorGraph::Node::Ptr midiOutputNode;

    AudioEngine (const AudioEngine&) = delete;
    AudioEngine& operator= (const AudioEngine&) = delete;
};

} // namespace dc
