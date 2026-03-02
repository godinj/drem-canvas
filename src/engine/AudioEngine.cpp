#include "AudioEngine.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"
#include <string>

namespace dc
{

// --- GraphCallback -----------------------------------------------------------
// Bridges dc::AudioCallback (float**) to dc::AudioGraph::processBlock().

class AudioEngine::GraphCallback : public dc::AudioCallback
{
public:
    explicit GraphCallback (dc::AudioGraph& graph)
        : graph_ (graph)
    {
    }

    void audioDeviceAboutToStart (double sampleRate, int bufferSize) override
    {
        graph_.prepare (sampleRate, bufferSize);
    }

    void audioCallback (const float** inputChannelData, int numInputChannels,
                        float** outputChannelData, int numOutputChannels,
                        int numSamples) override
    {
        // Wrap input channels as AudioBlock (const_cast is safe -- graph reads only)
        dc::AudioBlock inputBlock (const_cast<float**> (inputChannelData),
                                   numInputChannels, numSamples);

        // Wrap output channels as AudioBlock
        dc::AudioBlock outputBlock (outputChannelData, numOutputChannels, numSamples);
        outputBlock.clear();

        // Empty MIDI blocks for now (MIDI routing happens within the graph)
        dc::MidiBlock midiIn;
        dc::MidiBlock midiOut;

        graph_.processBlock (inputBlock, midiIn, outputBlock, midiOut, numSamples);
    }

    void audioDeviceStopped() override
    {
        graph_.release();
    }

private:
    dc::AudioGraph& graph_;
};

// --- AudioEngine -------------------------------------------------------------

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise (int numInputChannels, int numOutputChannels)
{
    deviceManager_ = dc::AudioDeviceManager::create();

    graphCallback_ = std::make_unique<GraphCallback> (graph_);
    deviceManager_->setCallback (graphCallback_.get());
    deviceManager_->openDefaultDevice (numInputChannels, numOutputChannels);

    double sampleRate = deviceManager_->isOpen() ? deviceManager_->getSampleRate() : 44100.0;
    int blockSize = deviceManager_->isOpen() ? deviceManager_->getBufferSize() : 512;

    graph_.prepare (sampleRate, blockSize);
}

void AudioEngine::shutdown()
{
    if (deviceManager_)
    {
        deviceManager_->closeDevice();
        deviceManager_->setCallback (nullptr);
    }

    graphCallback_.reset();
    graph_.release();
    graph_.clear();
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

NodeId AudioEngine::addProcessor (std::unique_ptr<AudioNode> processor)
{
    return graph_.addNode (std::move (processor));
}

void AudioEngine::removeProcessor (NodeId nodeId)
{
    graph_.removeNode (nodeId);
}

void AudioEngine::connectNodes (NodeId source, int sourceChannel,
                                NodeId dest, int destChannel)
{
    dc::Connection conn { source, sourceChannel, dest, destChannel };
    bool ok = graph_.addConnection (conn);

    if (! ok)
    {
        auto* srcNode = graph_.getNode (source);
        auto* dstNode = graph_.getNode (dest);
        std::string srcName = srcNode ? srcNode->getName() : "?";
        std::string dstName = dstNode ? dstNode->getName() : "?";

        fprintf (stderr, "AudioEngine: FAILED connection %s[%d] -> %s[%d]\n",
                 srcName.c_str(), sourceChannel,
                 dstName.c_str(), destChannel);
        fflush (stderr);
    }
}

} // namespace dc
