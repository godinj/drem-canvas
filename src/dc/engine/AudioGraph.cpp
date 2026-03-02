#include "dc/engine/AudioGraph.h"
#include "dc/foundation/assert.h"

#include <algorithm>
#include <queue>
#include <set>
#include <stack>

namespace dc {

// ─── I/O Terminal Nodes ────────────────────────────────────────────

namespace {

/// Pass-through node that copies graph input audio to its output.
class AudioInputNode : public AudioNode
{
public:
    void prepare (double /*sampleRate*/, int /*maxBlockSize*/) override {}
    void process (AudioBlock& /*audio*/, MidiBlock& /*midi*/, int /*numSamples*/) override {}
    std::string getName() const override { return "AudioInput"; }
};

/// Pass-through node that copies its input to graph output audio.
class AudioOutputNode : public AudioNode
{
public:
    void prepare (double /*sampleRate*/, int /*maxBlockSize*/) override {}
    void process (AudioBlock& /*audio*/, MidiBlock& /*midi*/, int /*numSamples*/) override {}
    std::string getName() const override { return "AudioOutput"; }
};

/// Pass-through node that copies graph input MIDI to its output.
class MidiInputNode : public AudioNode
{
public:
    void prepare (double /*sampleRate*/, int /*maxBlockSize*/) override {}
    void process (AudioBlock& /*audio*/, MidiBlock& /*midi*/, int /*numSamples*/) override {}
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    std::string getName() const override { return "MidiInput"; }
};

/// Pass-through node that copies its input to graph output MIDI.
class MidiOutputNode : public AudioNode
{
public:
    void prepare (double /*sampleRate*/, int /*maxBlockSize*/) override {}
    void process (AudioBlock& /*audio*/, MidiBlock& /*midi*/, int /*numSamples*/) override {}
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    std::string getName() const override { return "MidiOutput"; }
};

} // anonymous namespace

// ─── AudioGraph ────────────────────────────────────────────────────

AudioGraph::AudioGraph()
{
    // Create 4 I/O terminal nodes with sequential IDs 1-4
    {
        auto node = std::make_unique<AudioInputNode>();
        NodeId id = nextId_++;
        audioInputNodeId_ = id;
        NodeEntry entry;
        entry.node = std::move (node);
        entry.id = id;
        nodes_.emplace (id, std::move (entry));
    }
    {
        auto node = std::make_unique<AudioOutputNode>();
        NodeId id = nextId_++;
        audioOutputNodeId_ = id;
        NodeEntry entry;
        entry.node = std::move (node);
        entry.id = id;
        nodes_.emplace (id, std::move (entry));
    }
    {
        auto node = std::make_unique<MidiInputNode>();
        NodeId id = nextId_++;
        midiInputNodeId_ = id;
        NodeEntry entry;
        entry.node = std::move (node);
        entry.id = id;
        nodes_.emplace (id, std::move (entry));
    }
    {
        auto node = std::make_unique<MidiOutputNode>();
        NodeId id = nextId_++;
        midiOutputNodeId_ = id;
        NodeEntry entry;
        entry.node = std::move (node);
        entry.id = id;
        nodes_.emplace (id, std::move (entry));
    }

    // nextId_ is now 5
    orderDirty_ = true;
}

AudioGraph::~AudioGraph() = default;

// ─── Node Management ───────────────────────────────────────────────

NodeId AudioGraph::addNode (std::unique_ptr<AudioNode> node)
{
    NodeId id = nextId_++;
    NodeEntry entry;
    entry.node = std::move (node);
    entry.id = id;
    nodes_.emplace (id, std::move (entry));
    orderDirty_ = true;
    return id;
}

void AudioGraph::removeNode (NodeId id)
{
    // Do not allow removal of I/O terminal nodes
    if (id == audioInputNodeId_ || id == audioOutputNodeId_
        || id == midiInputNodeId_ || id == midiOutputNodeId_)
        return;

    disconnectNode (id);
    nodes_.erase (id);
    orderDirty_ = true;
}

AudioNode* AudioGraph::getNode (NodeId id) const
{
    auto it = nodes_.find (id);

    if (it == nodes_.end())
        return nullptr;

    return it->second.node.get();
}

// ─── Connection Management ─────────────────────────────────────────

bool AudioGraph::addConnection (const Connection& conn)
{
    // Validate both nodes exist
    if (nodes_.find (conn.sourceNode) == nodes_.end()
        || nodes_.find (conn.destNode) == nodes_.end())
        return false;

    // Check for cycles
    if (wouldCreateCycle (conn))
        return false;

    // Add to global connections list
    connections_.push_back (conn);

    // Add to source node's outputs
    nodes_[conn.sourceNode].outputs.push_back (conn);

    // Add to dest node's inputs
    nodes_[conn.destNode].inputs.push_back (conn);

    orderDirty_ = true;
    return true;
}

void AudioGraph::removeConnection (const Connection& conn)
{
    // Remove from global connections list
    connections_.erase (
        std::remove (connections_.begin(), connections_.end(), conn),
        connections_.end());

    // Remove from source node's outputs
    auto srcIt = nodes_.find (conn.sourceNode);

    if (srcIt != nodes_.end())
    {
        auto& outs = srcIt->second.outputs;
        outs.erase (std::remove (outs.begin(), outs.end(), conn), outs.end());
    }

    // Remove from dest node's inputs
    auto dstIt = nodes_.find (conn.destNode);

    if (dstIt != nodes_.end())
    {
        auto& ins = dstIt->second.inputs;
        ins.erase (std::remove (ins.begin(), ins.end(), conn), ins.end());
    }

    orderDirty_ = true;
}

void AudioGraph::disconnectNode (NodeId id)
{
    // Collect all connections involving this node
    std::vector<Connection> toRemove;

    for (auto& conn : connections_)
    {
        if (conn.sourceNode == id || conn.destNode == id)
            toRemove.push_back (conn);
    }

    for (auto& conn : toRemove)
        removeConnection (conn);
}

const std::vector<Connection>& AudioGraph::getConnections() const
{
    return connections_;
}

// ─── Processing ────────────────────────────────────────────────────

void AudioGraph::prepare (double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;

    // Prepare all nodes
    for (auto& [id, entry] : nodes_)
    {
        if (entry.node)
            entry.node->prepare (sampleRate, maxBlockSize);
    }

    // Rebuild processing order
    rebuildProcessingOrder();

    // Prepare buffer pool: number of nodes + 4 overhead
    bufferPool_.prepare (static_cast<int> (nodes_.size()) + 4, 2, maxBlockSize);
}

void AudioGraph::processBlock (AudioBlock& input, MidiBlock& midiIn,
                               AudioBlock& output, MidiBlock& midiOut,
                               int numSamples)
{
    if (orderDirty_)
        rebuildProcessingOrder();

    // Copy input into the audio input node's output buffer
    auto audioInIt = nodes_.find (audioInputNodeId_);

    if (audioInIt != nodes_.end())
        audioInIt->second.outputBuffer = input;

    // Copy midiIn into the MIDI input node's output MidiBlock
    auto midiInIt = nodes_.find (midiInputNodeId_);

    if (midiInIt != nodes_.end())
        midiInIt->second.outputMidi = midiIn;

    // Execute the graph
    executor_.execute (processingOrder_, nodes_, bufferPool_,
                       output, midiOut, numSamples);

    // Copy audio output node's result to graph output
    auto audioOutIt = nodes_.find (audioOutputNodeId_);

    if (audioOutIt != nodes_.end()
        && audioOutIt->second.outputBuffer.getNumChannels() > 0)
    {
        output.copyFrom (audioOutIt->second.outputBuffer);
    }

    // Copy MIDI output node's result to graph MIDI output
    auto midiOutIt = nodes_.find (midiOutputNodeId_);

    if (midiOutIt != nodes_.end())
    {
        midiOut.clear();

        for (auto event : midiOutIt->second.outputMidi)
            midiOut.addEvent (event.message, event.sampleOffset);
    }
}

void AudioGraph::release()
{
    for (auto& [id, entry] : nodes_)
    {
        if (entry.node)
            entry.node->release();
    }
}

// ─── Topology Queries ──────────────────────────────────────────────

const std::vector<NodeId>& AudioGraph::getProcessingOrder() const
{
    return processingOrder_;
}

bool AudioGraph::wouldCreateCycle (const Connection& conn) const
{
    // DFS from conn.destNode looking for conn.sourceNode
    // If we can reach the source from the dest, adding this edge creates a cycle.
    std::set<NodeId> visited;
    std::stack<NodeId> stack;
    stack.push (conn.destNode);

    while (! stack.empty())
    {
        NodeId current = stack.top();
        stack.pop();

        if (current == conn.sourceNode)
            return true;

        if (visited.count (current) > 0)
            continue;

        visited.insert (current);

        // Follow existing outgoing edges from current
        auto it = nodes_.find (current);

        if (it != nodes_.end())
        {
            for (auto& out : it->second.outputs)
                stack.push (out.destNode);
        }
    }

    return false;
}

// ─── Utility ───────────────────────────────────────────────────────

void AudioGraph::clear()
{
    // Collect non-I/O node IDs
    std::vector<NodeId> toRemove;

    for (auto& [id, entry] : nodes_)
    {
        if (id != audioInputNodeId_ && id != audioOutputNodeId_
            && id != midiInputNodeId_ && id != midiOutputNodeId_)
        {
            toRemove.push_back (id);
        }
    }

    // Remove all connections first
    connections_.clear();

    for (auto& [id, entry] : nodes_)
    {
        entry.inputs.clear();
        entry.outputs.clear();
    }

    // Remove non-I/O nodes
    for (auto id : toRemove)
        nodes_.erase (id);

    orderDirty_ = true;
}

// ─── Private ───────────────────────────────────────────────────────

void AudioGraph::rebuildProcessingOrder()
{
    // Kahn's algorithm for topological sort
    processingOrder_.clear();

    // 1. Compute in-degree for each node (count of unique source nodes in inputs)
    std::unordered_map<NodeId, int> inDegree;

    for (auto& [id, entry] : nodes_)
        inDegree[id] = 0;

    for (auto& [id, entry] : nodes_)
    {
        // Count unique source nodes feeding this node
        std::set<NodeId> uniqueSources;

        for (auto& conn : entry.inputs)
            uniqueSources.insert (conn.sourceNode);

        inDegree[id] = static_cast<int> (uniqueSources.size());
    }

    // 2. Enqueue nodes with in-degree 0
    std::queue<NodeId> readyQueue;

    for (auto& [id, degree] : inDegree)
    {
        if (degree == 0)
            readyQueue.push (id);
    }

    // 3. Process queue: dequeue, append to order, decrement in-degree of successors
    while (! readyQueue.empty())
    {
        NodeId current = readyQueue.front();
        readyQueue.pop();
        processingOrder_.push_back (current);

        auto it = nodes_.find (current);

        if (it == nodes_.end())
            continue;

        // Find all unique destination nodes from this node's outputs
        std::set<NodeId> uniqueDests;

        for (auto& conn : it->second.outputs)
            uniqueDests.insert (conn.destNode);

        for (auto destId : uniqueDests)
        {
            inDegree[destId]--;

            if (inDegree[destId] == 0)
                readyQueue.push (destId);
        }
    }

    // 4. Assert that all nodes are in the processing order (cycle = bug)
    dc_assert (processingOrder_.size() == nodes_.size());

    // 5. Mark order as clean
    orderDirty_ = false;
}

} // namespace dc
