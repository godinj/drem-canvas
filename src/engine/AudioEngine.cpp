#include "AudioEngine.h"
#include <string>
#include <vector>
#include <cstring>

namespace dc
{

// ─── GraphCallback ──────────────────────────────────────────────────
// Bridges dc::AudioCallback (float**) → juce::AudioProcessorGraph::processBlock().
// Zero-copy: wraps the raw float** from PortAudio directly into juce::AudioBuffer.

class AudioEngine::GraphCallback : public dc::AudioCallback
{
public:
    explicit GraphCallback (juce::AudioProcessorGraph& graph)
        : graph_ (graph)
    {
    }

    void audioDeviceAboutToStart (double sampleRate, int bufferSize) override
    {
        graph_.setPlayConfigDetails (numInputChannels_, numOutputChannels_,
                                     sampleRate, bufferSize);
        graph_.prepareToPlay (sampleRate, bufferSize);
    }

    void audioCallback (const float** inputChannelData, int numInputChannels,
                        float** outputChannelData, int numOutputChannels,
                        int numSamples) override
    {
        // Build a single channel pointer array: [inputs..., outputs...]
        int totalChannels = numInputChannels + numOutputChannels;

        // Use stack allocation for typical channel counts
        float* stackPtrs[16];
        float** channelPtrs = (totalChannels <= 16)
                                  ? stackPtrs
                                  : heapPtrs_.data();

        if (totalChannels > 16)
        {
            if (static_cast<int> (heapPtrs_.size()) < totalChannels)
                heapPtrs_.resize (static_cast<size_t> (totalChannels));
            channelPtrs = heapPtrs_.data();
        }

        // Copy input channel data into output buffers (graph reads inputs from the buffer)
        for (int ch = 0; ch < numInputChannels; ++ch)
        {
            // Write input data to the corresponding output channel pointer
            // so the graph can read it
            if (ch < numOutputChannels && inputChannelData != nullptr && inputChannelData[ch] != nullptr)
            {
                std::memcpy (outputChannelData[ch], inputChannelData[ch],
                             sizeof (float) * static_cast<size_t> (numSamples));
            }
            channelPtrs[ch] = (ch < numOutputChannels) ? outputChannelData[ch] : const_cast<float*> (inputChannelData[ch]);
        }
        for (int ch = numInputChannels; ch < totalChannels; ++ch)
        {
            int outIdx = ch - numInputChannels;
            if (outIdx < numOutputChannels)
            {
                // Clear output channels that don't have corresponding input
                if (ch >= numInputChannels)
                    std::memset (outputChannelData[outIdx], 0,
                                 sizeof (float) * static_cast<size_t> (numSamples));
                channelPtrs[ch] = outputChannelData[outIdx];
            }
        }

        // Zero-copy wrap: juce::AudioBuffer does NOT copy data when constructed
        // from float* const* (see juce_AudioSampleBuffer.h:91)
        juce::AudioBuffer<float> buffer (channelPtrs, totalChannels, numSamples);
        juce::MidiBuffer emptyMidi;

        graph_.processBlock (buffer, emptyMidi);

        // Output channels are already in outputChannelData (graph writes in-place)
        // Extract output from the buffer — the graph writes to channels
        // [numInputChannels .. totalChannels-1] which map to output
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            float* src = channelPtrs[numInputChannels + ch];
            if (src != outputChannelData[ch])
                std::memcpy (outputChannelData[ch], src,
                             sizeof (float) * static_cast<size_t> (numSamples));
        }
    }

    void audioDeviceStopped() override
    {
        graph_.releaseResources();
    }

    void setChannelCounts (int numIn, int numOut)
    {
        numInputChannels_ = numIn;
        numOutputChannels_ = numOut;
    }

private:
    juce::AudioProcessorGraph& graph_;
    int numInputChannels_ = 0;
    int numOutputChannels_ = 0;
    std::vector<float*> heapPtrs_;
};

// ─── AudioEngine ────────────────────────────────────────────────────

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
    deviceManager_ = dc::AudioDeviceManager::create();

    graphCallback_ = std::make_unique<GraphCallback> (*graph);
    graphCallback_->setChannelCounts (numInputChannels, numOutputChannels);
    deviceManager_->setCallback (graphCallback_.get());

    deviceManager_->openDefaultDevice (numInputChannels, numOutputChannels);

    double sampleRate = deviceManager_->isOpen() ? deviceManager_->getSampleRate() : 44100.0;
    int blockSize = deviceManager_->isOpen() ? deviceManager_->getBufferSize() : 512;

    // Set up graph I/O nodes
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
}

void AudioEngine::shutdown()
{
    if (deviceManager_)
    {
        deviceManager_->closeDevice();
        deviceManager_->setCallback (nullptr);
    }

    graphCallback_.reset();
    graph->clear();

    audioInputNode  = nullptr;
    audioOutputNode = nullptr;
    midiInputNode   = nullptr;
    midiOutputNode  = nullptr;

    deviceManager_.reset();
}

double AudioEngine::getSampleRate() const
{
    if (deviceManager_ && deviceManager_->isOpen())
        return deviceManager_->getSampleRate();
    return 44100.0;
}

int AudioEngine::getBufferSize() const
{
    if (deviceManager_ && deviceManager_->isOpen())
        return deviceManager_->getBufferSize();
    return 512;
}

std::string AudioEngine::getCurrentDeviceName() const
{
    if (deviceManager_)
        return deviceManager_->getCurrentDeviceName();
    return {};
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
    bool ok = graph->addConnection ({ { source, sourceChannel }, { dest, destChannel } });

    if (! ok)
    {
        std::string srcName = "?", dstName = "?";
        if (auto* srcNode = graph->getNodeForId (source))
            srcName = srcNode->getProcessor()->getName().toStdString();
        if (auto* dstNode = graph->getNodeForId (dest))
            dstName = dstNode->getProcessor()->getName().toStdString();

        fprintf (stderr, "AudioEngine: FAILED connection %s[%d] -> %s[%d]\n",
                 srcName.c_str(), sourceChannel,
                 dstName.c_str(), destChannel);
        fflush (stderr);
    }
}

} // namespace dc
