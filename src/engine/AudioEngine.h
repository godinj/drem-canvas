#pragma once
#include "dc/engine/AudioGraph.h"
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
    void stopStream();
    void shutdown();

    /** Suspend audio processing (callback outputs silence).
        Blocks until any in-flight callback has completed. */
    void suspendProcessing();

    /** Resume audio processing after a suspension. */
    void resumeProcessing();

    dc::AudioGraph& getGraph() { return graph_; }

    // Node management -- thin wrappers around dc::AudioGraph
    NodeId addProcessor (std::unique_ptr<AudioNode> processor);
    void removeProcessor (NodeId nodeId);
    void connectNodes (NodeId source, int sourceChannel,
                       NodeId dest, int destChannel);

    NodeId getAudioInputNodeId() const  { return graph_.getAudioInputNodeId(); }
    NodeId getAudioOutputNodeId() const { return graph_.getAudioOutputNodeId(); }

    // Device info accessors
    double getSampleRate() const;
    int getBufferSize() const;
    std::string getCurrentDeviceName() const;

private:
    class GraphCallback;

    std::unique_ptr<dc::AudioDeviceManager> deviceManager_;
    dc::AudioGraph graph_;
    std::unique_ptr<GraphCallback> graphCallback_;

    AudioEngine (const AudioEngine&) = delete;
    AudioEngine& operator= (const AudioEngine&) = delete;
};

} // namespace dc
