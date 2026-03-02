# 02 — Parallel Audio DAG Engine

> Replaces `juce::AudioProcessorGraph` with a custom directed acyclic graph (DAG)
> engine supporting parallel branch execution and zero-allocation audio processing.

**Phase**: 3 (Audio Graph)
**Dependencies**: Phase 0 (Foundation), Phase 2 (Audio I/O + MIDI types)
**Related**: [05-audio-io.md](05-audio-io.md), [06-midi-subsystem.md](06-midi-subsystem.md), [03-plugin-hosting.md](03-plugin-hosting.md)

---

## Overview

`juce::AudioProcessorGraph` manages a directed graph of `AudioProcessor` nodes,
topologically sorts them, and processes audio through the graph. Drem Canvas
currently uses it in `AudioEngine` to connect:

```
AudioInput → TrackProcessor(s) → MixBusProcessor → AudioOutput
                 ↓                      ↓
            MidiInput              MeterTapProcessor
```

Each `TrackProcessor` contains an internal plugin chain (serial).

The replacement provides:
1. **Minimal node interface** — `dc::AudioNode` (much simpler than `juce::AudioProcessor`)
2. **Topology management** — adjacency list with incremental topological sort
3. **Buffer pooling** — pre-allocated buffers recycled per block (zero alloc on audio thread)
4. **Parallel execution** — independent branches processed in parallel via work-stealing
5. **Plugin Delay Compensation** — latency reporting and automatic delay insertion

---

## Current JUCE Graph Usage

### AudioEngine (src/engine/AudioEngine.h)

```cpp
juce::AudioProcessorGraph graph;
juce::AudioProcessorGraph::Node::Ptr audioInputNode;
juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
juce::AudioProcessorGraph::Node::Ptr midiInputNode;
juce::AudioProcessorGraph::Node::Ptr midiOutputNode;
juce::AudioProcessorGraph::Node::Ptr masterBusNode;
std::vector<juce::AudioProcessorGraph::Node::Ptr> trackNodes;
```

### AudioProcessor Subclasses (12 total)

| Processor | Channels | MIDI | Purpose |
|-----------|----------|------|---------|
| `TrackProcessor` | Stereo | Yes | Per-track playback, plugin chain |
| `MixBusProcessor` | Stereo | No | Master bus summing |
| `MeterTapProcessor` | Stereo | No | Level metering (RMS/peak) |
| `MetronomeProcessor` | Stereo | No | Click track synthesis |
| `MidiClipProcessor` | 0 audio | Yes | MIDI sequence playback |
| `StepSequencerProcessor` | 0 audio | Yes | Step pattern to MIDI |
| `SimpleSynthProcessor` | Stereo | Yes | Fallback sine synth |
| `AudioRecorder` | Stereo | No | Disk recording |
| `BounceProcessor` | Stereo | No | Offline render |
| `MidiEngine` | 0 audio | Yes | MIDI device routing |
| `AudioGraphIOProcessor` | varies | varies | Graph I/O terminals (JUCE built-in) |

---

## Design: `dc::AudioNode`

Minimal interface replacing `juce::AudioProcessor`. Deliberately simpler —
no parameter system, no editor, no bus layout negotiation.

```cpp
namespace dc {

class AudioNode
{
public:
    virtual ~AudioNode() = default;

    /// Called once when the graph is prepared (sample rate, block size known)
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    /// Called once when the graph is released
    virtual void release() {}

    /// Process one block of audio and MIDI.
    /// audio: interleaved channel buffers (read/write)
    /// midi: timestamped MIDI events for this block (read/write)
    /// numSamples: number of samples to process (<= maxBlockSize)
    virtual void process(AudioBlock& audio, MidiBlock& midi,
                         int numSamples) = 0;

    /// Report latency in samples (for PDC). Default: 0.
    virtual int getLatencySamples() const { return 0; }

    /// Number of input/output audio channels
    virtual int getNumInputChannels() const { return 2; }
    virtual int getNumOutputChannels() const { return 2; }

    /// Whether this node accepts/produces MIDI
    virtual bool acceptsMidi() const { return false; }
    virtual bool producesMidi() const { return false; }

    /// Human-readable name (for debugging/display)
    virtual std::string getName() const { return "AudioNode"; }
};

} // namespace dc
```

### dc::AudioBlock

Non-owning view over audio sample buffers. Replaces `juce::AudioBuffer<float>`
in the processing interface.

```cpp
namespace dc {

class AudioBlock
{
public:
    AudioBlock() = default;
    AudioBlock(float** channelData, int numChannels, int numSamples);

    float* getChannel(int ch);
    const float* getChannel(int ch) const;
    int getNumChannels() const;
    int getNumSamples() const;

    /// Zero all channels
    void clear();

    /// Zero a range of samples
    void clear(int startSample, int numSamples);

    /// Add samples from another block (mixing)
    void addFrom(const AudioBlock& source);
    void addFrom(int destChannel, const AudioBlock& source,
                 int sourceChannel, int numSamples, float gain = 1.0f);

    /// Copy samples from another block
    void copyFrom(const AudioBlock& source);
    void copyFrom(int destChannel, const AudioBlock& source,
                  int sourceChannel, int numSamples);

    /// Apply gain to all channels
    void applyGain(float gain);
    void applyGain(int channel, int startSample, int numSamples, float gain);

    /// Get a sub-block (offset into existing buffers)
    AudioBlock getSubBlock(int startSample, int numSamples) const;

private:
    float** data_ = nullptr;
    int numChannels_ = 0;
    int numSamples_ = 0;
};

} // namespace dc
```

### dc::MidiBlock

Non-owning view over a timestamped MIDI event buffer. Replaces `juce::MidiBuffer`
in the processing interface. See [06-midi-subsystem.md](06-midi-subsystem.md) for
`dc::MidiMessage` and `dc::MidiBuffer` details.

```cpp
namespace dc {

class MidiBlock
{
public:
    MidiBlock();
    MidiBlock(MidiBuffer& buffer);

    /// Iterate events in timestamp order
    MidiBuffer::Iterator begin() const;
    MidiBuffer::Iterator end() const;

    /// Add an event at the given sample offset
    void addEvent(const MidiMessage& msg, int sampleOffset);

    /// Remove all events
    void clear();

    /// Number of events
    int getNumEvents() const;

    /// Whether the block has any events
    bool isEmpty() const;

private:
    MidiBuffer* buffer_ = nullptr;
};

} // namespace dc
```

---

## Design: `dc::AudioGraph`

### Topology

```cpp
namespace dc {

using NodeId = uint32_t;

class AudioGraph
{
public:
    AudioGraph();
    ~AudioGraph();

    // --- Node management ---

    /// Add a node to the graph. Returns its ID.
    NodeId addNode(std::unique_ptr<AudioNode> node);

    /// Remove a node and all its connections.
    void removeNode(NodeId id);

    /// Get a node by ID (nullptr if not found)
    AudioNode* getNode(NodeId id) const;

    // --- Connection management ---

    struct Connection
    {
        NodeId sourceNode;
        int sourceChannel;   // -1 = MIDI
        NodeId destNode;
        int destChannel;     // -1 = MIDI
    };

    /// Add an audio or MIDI connection between nodes.
    /// sourceChannel/destChannel = -1 for MIDI connections.
    bool addConnection(const Connection& conn);

    /// Remove a connection.
    void removeConnection(const Connection& conn);

    /// Remove all connections involving a node.
    void disconnectNode(NodeId id);

    /// Get all connections
    const std::vector<Connection>& getConnections() const;

    // --- I/O terminals ---

    /// Special node IDs for graph I/O
    NodeId getAudioInputNodeId() const;
    NodeId getAudioOutputNodeId() const;
    NodeId getMidiInputNodeId() const;
    NodeId getMidiOutputNodeId() const;

    // --- Processing ---

    /// Prepare all nodes for processing
    void prepare(double sampleRate, int maxBlockSize);

    /// Process one block through the entire graph
    void processBlock(AudioBlock& input, MidiBlock& midiIn,
                      AudioBlock& output, MidiBlock& midiOut,
                      int numSamples);

    /// Release all nodes
    void release();

    // --- Topology queries ---

    /// Get the processing order (topologically sorted node IDs)
    const std::vector<NodeId>& getProcessingOrder() const;

    /// Check if adding a connection would create a cycle
    bool wouldCreateCycle(const Connection& conn) const;

private:
    struct NodeEntry
    {
        std::unique_ptr<AudioNode> node;
        NodeId id;
        std::vector<Connection> inputs;   // connections feeding this node
        std::vector<Connection> outputs;  // connections from this node
        int latencySamples = 0;           // reported + compensated
    };

    std::unordered_map<NodeId, NodeEntry> nodes_;
    std::vector<Connection> connections_;
    std::vector<NodeId> processingOrder_;  // topological sort result
    NodeId nextId_ = 1;
    bool orderDirty_ = true;

    void rebuildProcessingOrder();
    GraphExecutor executor_;
    BufferPool bufferPool_;
};

} // namespace dc
```

### Incremental Topological Sort

The graph maintains a topological ordering of nodes. When connections change,
the order is rebuilt using Kahn's algorithm:

```
1. Compute in-degree for each node
2. Enqueue nodes with in-degree 0
3. Process queue: dequeue node, append to order, decrement in-degree of successors
4. If order.size() != nodes.size(), cycle detected (error)
```

The sort is incremental: `orderDirty_` flag defers rebuild until the next
`processBlock()` call. Multiple topology changes batch into one sort.

### Parallel Execution Levels

Nodes at the same topological level (no dependencies between them) can execute
in parallel:

```
Level 0: AudioInput, MidiInput
Level 1: Track1, Track2, Track3, Metronome    ← parallel
Level 2: MixBus                                ← waits for all tracks
Level 3: MeterTap, AudioOutput                ← parallel
```

---

## Design: `dc::BufferPool`

Pre-allocates audio buffers during `prepare()`. Nodes check out buffers from the
pool during processing and return them when done. Zero allocations on the audio thread.

```cpp
namespace dc {

class BufferPool
{
public:
    /// Prepare the pool with enough buffers for the graph
    void prepare(int numBuffers, int numChannels, int maxBlockSize);

    /// Check out a buffer (audio thread, lock-free)
    AudioBlock acquire(int numChannels, int numSamples);

    /// Return a buffer to the pool
    void release(AudioBlock block);

    /// Release all buffers (called at end of processBlock)
    void releaseAll();

private:
    struct Buffer
    {
        std::vector<float> storage;      // flat interleaved
        std::vector<float*> channelPtrs; // pointers into storage
        std::atomic<bool> inUse{false};
    };

    std::vector<Buffer> buffers_;
    int maxChannels_ = 0;
    int maxBlockSize_ = 0;
};

} // namespace dc
```

The pool size is determined by the graph topology: at minimum, one buffer per
node that has active connections, plus overhead for parallel execution.

---

## Design: `dc::GraphExecutor`

Executes the topologically-sorted graph, potentially in parallel.

```cpp
namespace dc {

class GraphExecutor
{
public:
    GraphExecutor();
    ~GraphExecutor();

    /// Set the number of worker threads (0 = single-threaded)
    void setNumThreads(int numThreads);

    /// Execute the graph for one block
    void execute(const std::vector<NodeId>& order,
                 std::unordered_map<NodeId, NodeEntry>& nodes,
                 BufferPool& pool,
                 int numSamples);

private:
    // Worker threads + work queue for parallel execution
    std::vector<std::thread> workers_;
    // Per-node dependency counters (atomic, decremented as predecessors complete)
    std::vector<std::atomic<int>> depCounters_;
    // Lock-free queue of ready-to-execute nodes
    // ...
};
```

### Single-Threaded Path (Initial Implementation)

For the initial migration, the executor processes nodes sequentially in
topological order. This is functionally equivalent to JUCE's current behavior.

```cpp
void GraphExecutor::execute(...)
{
    for (auto nodeId : order)
    {
        auto& entry = nodes[nodeId];
        AudioBlock block = pool.acquire(entry.node->getNumOutputChannels(), numSamples);
        MidiBlock midi = ...; // collect from input connections

        // Mix input connections into block
        for (auto& conn : entry.inputs)
            block.addFrom(getOutputBuffer(conn.sourceNode));

        entry.node->process(block, midi, numSamples);
        entry.outputBuffer = block;
    }
}
```

### Parallel Path (Future Enhancement)

Work-stealing scheduler where:
1. Nodes with zero unprocessed dependencies are added to a ready queue
2. Worker threads dequeue and process nodes
3. When a node completes, decrement dependency counters of successors
4. Successors with zero remaining dependencies are enqueued

This parallelizes independent branches (e.g., all track processors in Level 1).

---

## Plugin Delay Compensation (PDC)

### Latency Reporting

Each `AudioNode` reports its latency via `getLatencySamples()`. Plugin nodes
query the underlying VST3 component's `getLatencySamples()`.

### Compensation Strategy

When any node in a parallel branch reports latency, the graph inserts delay
buffers on shorter branches to align them at the merge point:

```
Track1 (0 samples latency) ─── [+256 delay] ──┐
Track2 (256 samples latency) ──────────────────┤── MixBus
Track3 (0 samples latency) ─── [+256 delay] ──┘
```

### Implementation

1. After topological sort, compute max latency for each merge point
2. For each input to a merge point, compute the gap between its cumulative
   latency and the max
3. Insert a `dc::DelayNode` on the connection with the appropriate delay
4. When latency changes (plugin swap), recalculate and update delay nodes

```cpp
class DelayNode : public AudioNode
{
public:
    void setDelay(int samples);
    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override;

private:
    std::vector<float> delayBuffer_;
    int delaySamples_ = 0;
    int writePos_ = 0;
};
```

---

## Migration: Existing Processors → AudioNode

Each existing `juce::AudioProcessor` subclass is converted to `dc::AudioNode`:

| Processor | Key changes |
|-----------|------------|
| `TrackProcessor` | `processBlock(AudioBuffer, MidiBuffer)` → `process(AudioBlock, MidiBlock, int)`. Remove `prepareToPlay`/`releaseResources` boilerplate. Internal plugin chain becomes sub-graph or sequential node list. |
| `MixBusProcessor` | Simple: sum inputs, apply master gain. Remove AudioProcessor overhead. |
| `MeterTapProcessor` | Read-only tap: compute RMS/peak, store in atomics. Remove AudioProcessor overhead. |
| `MetronomeProcessor` | Sine/noise synthesis. Remove AudioProcessor base. |
| `MidiClipProcessor` | MIDI-only node (0 audio channels). Reads sequence, emits to MidiBlock. |
| `StepSequencerProcessor` | MIDI-only. Pattern → MidiBlock. |
| `SimpleSynthProcessor` | Stereo + MIDI. Sine wave synth. |
| `AudioRecorder` | Stereo input, writes to disk via ring buffer. Uses dc::ThreadedRecorder. |
| `BounceProcessor` | Offline render: accumulates output to file. |
| `MidiEngine` | MIDI device routing. Uses dc::MidiDeviceManager. |

### AudioProcessor Boilerplate Eliminated

Each `AudioProcessor` subclass currently must implement ~20 virtual methods, most
returning default values:

```cpp
// All of these go away with dc::AudioNode:
const String getName() const override;
bool acceptsMidi() const override;
bool producesMidi() const override;
double getTailLengthSeconds() const override;
int getNumPrograms() const override;
int getCurrentProgram() const override;
void setCurrentProgram(int) override;
const String getProgramName(int) const override;
void changeProgramName(int, const String&) override;
void getStateInformation(MemoryBlock&) override;
void setStateInformation(const void*, int) override;
bool hasEditor() const override;
AudioProcessorEditor* createEditor() override;
```

`dc::AudioNode` requires only `prepare()`, `process()`, and optionally
`release()` + `getLatencySamples()`.

---

## Graph Lifecycle

### Prepare Phase (on message thread)

```cpp
audioGraph.prepare(sampleRate, blockSize);
// → allocates BufferPool
// → calls prepare() on each node
// → computes topological order
// → computes PDC delays
```

### Process Phase (on audio thread)

```cpp
// Called from audio callback (PortAudio)
audioGraph.processBlock(inputBlock, midiIn, outputBlock, midiOut, numSamples);
// → executor processes nodes in order
// → buffer pool manages temporaries
// → zero allocations
```

### Topology Changes (on message thread)

```cpp
// Add/remove nodes and connections on message thread
auto id = audioGraph.addNode(std::make_unique<TrackProcessor>(...));
audioGraph.addConnection({audioInputId, 0, id, 0});
audioGraph.addConnection({audioInputId, 1, id, 1});
audioGraph.addConnection({id, 0, mixBusId, 0});
audioGraph.addConnection({id, 1, mixBusId, 1});
// → marks order dirty, rebuilt on next processBlock
```

Thread safety for topology changes while audio is running: use a double-buffer
or swap approach. Build new topology on message thread, atomically swap into
the audio thread's view.

---

## Testing Strategy

1. **Unit test**: Single node graph, process one block, verify output
2. **Chain test**: A→B→C serial chain, verify signal passes through
3. **Parallel test**: A+B→C merge, verify both inputs summed
4. **PDC test**: Nodes with different latencies, verify alignment at merge
5. **Cycle detection**: Attempt cyclic connection, verify rejection
6. **Buffer pool**: Process 1000 blocks, verify zero allocations (custom allocator tracking)
7. **Topology change**: Add/remove nodes while processing, verify no crashes
8. **Regression**: Compare output of JUCE graph vs dc::AudioGraph for same session
