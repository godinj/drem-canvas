#include "dc/engine/GraphExecutor.h"
#include "dc/engine/AudioGraph.h"
#include "dc/engine/AudioNode.h"
#include "dc/engine/BufferPool.h"
#include "dc/engine/MidiBlock.h"
#include "dc/audio/AudioBlock.h"

#include <algorithm>

namespace dc {

void GraphExecutor::execute (const std::vector<uint32_t>& processingOrder,
                             std::unordered_map<uint32_t, NodeEntry>& nodes,
                             BufferPool& pool,
                             AudioBlock& graphOutput,
                             MidiBlock& graphMidiOut,
                             int numSamples)
{
    for (auto nodeId : processingOrder)
    {
        auto it = nodes.find (nodeId);

        if (it == nodes.end())
            continue;

        auto& entry = it->second;
        auto* node = entry.node.get();

        if (node == nullptr)
            continue;

        // 1. Acquire an output buffer for this node
        AudioBlock block = pool.acquire (node->getNumOutputChannels(), numSamples);

        // 2. Create a MidiBlock for this node's MIDI I/O
        MidiBlock midi;
        midi.clear();

        // 3. Mix audio input connections into the block
        for (auto& conn : entry.inputs)
        {
            if (conn.sourceChannel < 0 || conn.destChannel < 0)
                continue;  // MIDI connection, handled below

            auto srcIt = nodes.find (conn.sourceNode);

            if (srcIt == nodes.end())
                continue;

            auto& srcEntry = srcIt->second;

            if (srcEntry.outputBuffer.getNumChannels() == 0)
                continue;

            int srcCh = conn.sourceChannel;
            int dstCh = conn.destChannel;

            if (srcCh < srcEntry.outputBuffer.getNumChannels() &&
                dstCh < block.getNumChannels())
            {
                block.addFrom (dstCh, srcEntry.outputBuffer, srcCh, numSamples);
            }
        }

        // 4. Collect MIDI input connections (channel == -1)
        for (auto& conn : entry.inputs)
        {
            if (conn.sourceChannel != -1 || conn.destChannel != -1)
                continue;  // Audio connection, already handled

            auto srcIt = nodes.find (conn.sourceNode);

            if (srcIt == nodes.end())
                continue;

            auto& srcEntry = srcIt->second;

            // Copy events from source node's output MidiBlock
            for (auto event : srcEntry.outputMidi)
                midi.addEvent (event.message, event.sampleOffset);
        }

        // 5. Process the node
        node->process (block, midi, numSamples);

        // 6. Store output buffer/midi in entry for downstream nodes
        entry.outputBuffer = block;
        entry.outputMidi = midi;
    }

    // 7. Copy graph output from the audio output node
    //    (The AudioGraph::processBlock handles this by reading the output node's buffer)

    // 8. Release all pool buffers for reuse next block
    pool.releaseAll();
}

} // namespace dc
