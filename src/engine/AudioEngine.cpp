#include "AudioEngine.h"

namespace dc
{

AudioEngine::AudioEngine()
    : graph (std::make_unique<juce::AudioProcessorGraph>())
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise (int numInputChannels, int numOutputChannels)
{
    deviceManager.initialiseWithDefaultDevices (numInputChannels, numOutputChannels);

    auto* device = deviceManager.getCurrentAudioDevice();

    double sampleRate = 44100.0;
    int blockSize = 512;

    if (device != nullptr)
    {
        sampleRate = device->getCurrentSampleRate();
        blockSize  = device->getCurrentBufferSizeSamples();
    }

    graph->setPlayConfigDetails (numInputChannels, numOutputChannels, sampleRate, blockSize);
    graph->prepareToPlay (sampleRate, blockSize);

    audioInputNode  = graph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                          juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    audioOutputNode = graph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                          juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    midiInputNode   = graph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                          juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
    midiOutputNode  = graph->addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                          juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode));

    player.setProcessor (graph.get());
    deviceManager.addAudioCallback (&player);
}

void AudioEngine::shutdown()
{
    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
    graph->clear();

    audioInputNode  = nullptr;
    audioOutputNode = nullptr;
    midiInputNode   = nullptr;
    midiOutputNode  = nullptr;
}

juce::AudioProcessorGraph::Node::Ptr AudioEngine::addProcessor (std::unique_ptr<juce::AudioProcessor> processor)
{
    return graph->addNode (std::move (processor));
}

void AudioEngine::removeProcessor (juce::AudioProcessorGraph::NodeID nodeId)
{
    graph->removeNode (nodeId);
}

void AudioEngine::connectNodes (juce::AudioProcessorGraph::NodeID source, int sourceChannel,
                                juce::AudioProcessorGraph::NodeID dest, int destChannel)
{
    graph->addConnection ({ { source, sourceChannel }, { dest, destChannel } });
}

} // namespace dc
