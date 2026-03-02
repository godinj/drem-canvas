#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

class AudioNode;
class AudioBlock;
class MidiBlock;
class BufferPool;

struct NodeEntry;  // forward-declared, defined in AudioGraph.h

/// Executes the topologically-sorted graph. Single-threaded for now.
/// (Parallel execution via work-stealing is a future enhancement.)
class GraphExecutor
{
public:
    GraphExecutor() = default;

    /// Execute the graph for one block.
    /// processingOrder: topologically sorted node IDs
    /// nodes: map from NodeId to NodeEntry (contains AudioNode + connections)
    /// pool: buffer pool for intermediate buffers
    /// graphOutput: the final output block to write to
    /// graphMidiOut: the final MIDI output block
    void execute (const std::vector<uint32_t>& processingOrder,
                  std::unordered_map<uint32_t, NodeEntry>& nodes,
                  BufferPool& pool,
                  AudioBlock& graphOutput,
                  MidiBlock& graphMidiOut,
                  int numSamples);
};

} // namespace dc
