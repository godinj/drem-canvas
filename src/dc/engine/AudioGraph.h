#pragma once

#include "dc/engine/AudioNode.h"
#include "dc/engine/BufferPool.h"
#include "dc/engine/GraphExecutor.h"
#include "dc/engine/MidiBlock.h"
#include "dc/audio/AudioBlock.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

using NodeId = uint32_t;

struct Connection
{
    NodeId sourceNode;
    int sourceChannel;   // -1 = MIDI
    NodeId destNode;
    int destChannel;     // -1 = MIDI

    bool operator== (const Connection& other) const
    {
        return sourceNode == other.sourceNode
            && sourceChannel == other.sourceChannel
            && destNode == other.destNode
            && destChannel == other.destChannel;
    }
};

struct NodeEntry
{
    std::unique_ptr<AudioNode> node;
    NodeId id = 0;
    std::vector<Connection> inputs;
    std::vector<Connection> outputs;
    int latencySamples = 0;

    // Runtime state (set during execution, not serialized)
    AudioBlock outputBuffer;    // set by executor after process()
    MidiBlock outputMidi;       // set by executor after process()
};

/// Main audio graph container. Manages topology (nodes + connections),
/// topological sort, buffer pooling, and single-threaded processing dispatch.
class AudioGraph
{
public:
    AudioGraph();
    ~AudioGraph();

    // --- Node management ---
    NodeId addNode (std::unique_ptr<AudioNode> node);
    void removeNode (NodeId id);
    AudioNode* getNode (NodeId id) const;

    // --- Connection management ---
    bool addConnection (const Connection& conn);
    void removeConnection (const Connection& conn);
    void disconnectNode (NodeId id);
    const std::vector<Connection>& getConnections() const;

    // --- I/O terminal node IDs ---
    NodeId getAudioInputNodeId() const  { return audioInputNodeId_; }
    NodeId getAudioOutputNodeId() const { return audioOutputNodeId_; }
    NodeId getMidiInputNodeId() const   { return midiInputNodeId_; }
    NodeId getMidiOutputNodeId() const  { return midiOutputNodeId_; }

    // --- Processing ---
    void prepare (double sampleRate, int maxBlockSize);
    void processBlock (AudioBlock& input, MidiBlock& midiIn,
                       AudioBlock& output, MidiBlock& midiOut,
                       int numSamples);
    void release();

    // --- Topology queries ---
    const std::vector<NodeId>& getProcessingOrder() const;
    bool wouldCreateCycle (const Connection& conn) const;

    // --- Utility ---
    void clear();

private:
    std::unordered_map<NodeId, NodeEntry> nodes_;
    std::vector<Connection> connections_;
    std::vector<NodeId> processingOrder_;
    NodeId nextId_ = 1;
    bool orderDirty_ = true;

    // I/O terminal nodes (created in constructor)
    NodeId audioInputNodeId_ = 0;
    NodeId audioOutputNodeId_ = 0;
    NodeId midiInputNodeId_ = 0;
    NodeId midiOutputNodeId_ = 0;

    GraphExecutor executor_;
    BufferPool bufferPool_;

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 512;

    void rebuildProcessingOrder();
};

} // namespace dc
