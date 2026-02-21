#pragma once
#include <JuceHeader.h>

namespace dc
{

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    void initialise (int numInputChannels, int numOutputChannels);
    void shutdown();

    juce::AudioDeviceManager& getDeviceManager()    { return deviceManager; }
    juce::AudioProcessorGraph& getGraph()            { return *graph; }

    // Node management
    juce::AudioProcessorGraph::Node::Ptr addProcessor (std::unique_ptr<juce::AudioProcessor> processor);
    void removeProcessor (juce::AudioProcessorGraph::NodeID nodeId);
    void connectNodes (juce::AudioProcessorGraph::NodeID source, int sourceChannel,
                       juce::AudioProcessorGraph::NodeID dest, int destChannel);

    juce::AudioProcessorGraph::Node::Ptr getAudioInputNode()  const { return audioInputNode; }
    juce::AudioProcessorGraph::Node::Ptr getAudioOutputNode() const { return audioOutputNode; }

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorPlayer player;

    juce::AudioProcessorGraph::Node::Ptr audioInputNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::AudioProcessorGraph::Node::Ptr midiOutputNode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace dc
