# Agent: Graph Infrastructure

You are working on the `feature/sans-juce-audio-graph` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 3 of the Sans-JUCE migration: create the `dc::engine` library — the
core audio graph infrastructure that replaces `juce::AudioProcessorGraph`.

## Context

Read these specs before starting:
- `docs/sans-juce/02-audio-graph.md` (full graph engine design)
- `docs/sans-juce/08-migration-guide.md` (Phase 3 section)
- `src/dc/audio/AudioBlock.h` (existing AudioBlock — enhance in place, don't duplicate)
- `src/dc/midi/MidiBuffer.h` (existing MidiBuffer — MidiBlock wraps this)
- `src/dc/foundation/spsc_queue.h` (existing lock-free queue — reuse for future parallel executor)

## Deliverables

### Enhance existing file

#### 1. `src/dc/audio/AudioBlock.h`

Add missing methods to the existing AudioBlock class. Keep it header-only.
Preserve the existing constructors and methods. Add:

- Default constructor: `AudioBlock() = default;`
- `void clear(int startSample, int numSamples)` — zero a range of samples on all channels
- `void addFrom(const AudioBlock& source)` — add all channels from source (mix)
- `void addFrom(int destChannel, const AudioBlock& source, int sourceChannel, int numSamples, float gain = 1.0f)` — add one channel with gain
- `void copyFrom(const AudioBlock& source)` — copy all channels from source
- `void copyFrom(int destChannel, const AudioBlock& source, int sourceChannel, int numSamples)` — copy one channel
- `void applyGain(float gain)` — apply gain to all channels, all samples
- `void applyGain(int channel, int startSample, int numSamples, float gain)` — apply gain to a range
- `AudioBlock getSubBlock(int startSample, int numSamples) const` — return a view offset into existing buffers

All methods are inline. Use `std::memcpy` for copy, sample-by-sample loop for add/gain.
`getSubBlock` returns a new AudioBlock pointing into the same channel arrays at an offset:

```cpp
AudioBlock getSubBlock(int startSample, int numSamples) const
{
    // Build offset pointer array — use a small static thread_local buffer
    // to avoid allocation. Max 32 channels.
    static thread_local float* offsetPtrs[32];
    int ch = std::min(numChannels_, 32);
    for (int i = 0; i < ch; ++i)
        offsetPtrs[i] = channels_[i] + startSample;
    return AudioBlock(offsetPtrs, ch, numSamples);
}
```

**Important**: The existing `float** channels_` member is private. The new methods
are member functions so they have access. Do not change the member visibility.

### New files (`src/dc/engine/`)

#### 2. `AudioNode.h`

Pure virtual interface replacing `juce::AudioProcessor`. Header-only, no `.cpp`.

```cpp
#pragma once
#include <string>

namespace dc {

class AudioBlock;
class MidiBlock;

class AudioNode
{
public:
    virtual ~AudioNode() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void release() {}
    virtual void process(AudioBlock& audio, MidiBlock& midi, int numSamples) = 0;

    virtual int getLatencySamples() const { return 0; }
    virtual int getNumInputChannels() const { return 2; }
    virtual int getNumOutputChannels() const { return 2; }
    virtual bool acceptsMidi() const { return false; }
    virtual bool producesMidi() const { return false; }
    virtual std::string getName() const { return "AudioNode"; }
};

} // namespace dc
```

Forward-declare `AudioBlock` and `MidiBlock`; include their headers only from `.cpp` files
or from files that use the actual types.

#### 3. `MidiBlock.h` / `MidiBlock.cpp`

Non-owning view over a `dc::MidiBuffer`. Thin wrapper that delegates to the underlying buffer.

**Header:**
```cpp
#pragma once
#include "dc/midi/MidiBuffer.h"

namespace dc {

class MidiBlock
{
public:
    MidiBlock();
    explicit MidiBlock(MidiBuffer& buffer);

    MidiBuffer::Iterator begin() const;
    MidiBuffer::Iterator end() const;

    void addEvent(const MidiMessage& msg, int sampleOffset);
    void clear();
    int getNumEvents() const;
    bool isEmpty() const;

    /// Get the underlying buffer (for passing to legacy code)
    MidiBuffer* getBuffer() { return buffer_; }

private:
    MidiBuffer* buffer_ = nullptr;
    MidiBuffer ownedBuffer_;  // used when constructed with default ctor
};

} // namespace dc
```

**Implementation notes:**
- Default constructor: set `buffer_ = &ownedBuffer_` so a default-constructed MidiBlock
  owns its own storage (processor code can create local MidiBlocks without an external buffer)
- `MidiBlock(MidiBuffer& buffer)`: set `buffer_ = &buffer`
- All methods delegate to `buffer_->...`

#### 4. `BufferPool.h` / `BufferPool.cpp`

Pre-allocated audio buffer pool. Zero allocation on the audio thread.

**Header:**
```cpp
#pragma once
#include "dc/audio/AudioBlock.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace dc {

class BufferPool
{
public:
    void prepare(int numBuffers, int numChannels, int maxBlockSize);

    /// Acquire a buffer (audio thread, lock-free).
    /// Returns a zeroed AudioBlock. Asserts if pool is exhausted.
    AudioBlock acquire(int numChannels, int numSamples);

    /// Release a specific buffer back to the pool.
    void release(AudioBlock block);

    /// Release all buffers (called at end of processBlock).
    void releaseAll();

private:
    struct Buffer
    {
        std::vector<float> storage;       // flat: numChannels * maxBlockSize
        std::vector<float*> channelPtrs;  // pointers into storage
        std::atomic<bool> inUse{false};
    };

    std::vector<Buffer> buffers_;
    int maxChannels_ = 0;
    int maxBlockSize_ = 0;
};

} // namespace dc
```

**Implementation notes:**
- `prepare`: Allocate `numBuffers` entries. Each `storage` has `numChannels * maxBlockSize`
  floats. Each `channelPtrs[ch]` points to `storage.data() + ch * maxBlockSize`.
- `acquire`: Linear scan for first buffer with `inUse == false`. Set `inUse = true`.
  Zero the requested region (`numChannels * numSamples` floats). Return AudioBlock
  wrapping the `channelPtrs`. Use `dc_assert` if no buffer available (pool exhausted
  = graph topology bug, not a runtime condition).
- `release`: Find the buffer whose `channelPtrs[0]` matches `block.getChannel(0)`.
  Set `inUse = false`.
- `releaseAll`: Set all `inUse = false`.
- Use `std::atomic_bool` with `memory_order_relaxed` — the pool is only accessed from
  the audio thread (single-threaded executor).

#### 5. `GraphExecutor.h` / `GraphExecutor.cpp`

Executes the topologically-sorted graph. **Single-threaded for now** (parallel is a
future enhancement — do NOT implement the thread pool).

**Header:**
```cpp
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
    void execute(const std::vector<uint32_t>& processingOrder,
                 std::unordered_map<uint32_t, NodeEntry>& nodes,
                 BufferPool& pool,
                 AudioBlock& graphOutput,
                 MidiBlock& graphMidiOut,
                 int numSamples);
};

} // namespace dc
```

**Implementation:**
For each node in processing order:
1. Acquire a buffer from the pool (`node.getNumOutputChannels()`, `numSamples`)
2. Clear the buffer
3. Mix input connections: for each audio input connection, `addFrom` the source node's
   output buffer into this node's input buffer
4. Collect MIDI: for each MIDI input connection (channel == -1), copy events from
   source node's output MidiBlock into this node's input MidiBlock
5. Call `node->process(block, midi, numSamples)`
6. Store the output buffer reference in the NodeEntry for downstream nodes to read
7. For the graph output node, copy to `graphOutput` / `graphMidiOut`
8. At the end, call `pool.releaseAll()`

#### 6. `AudioGraph.h` / `AudioGraph.cpp`

Main graph container. Topology management + topological sort + processing dispatch.

**Header:**
```cpp
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

    bool operator==(const Connection& other) const;
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

class AudioGraph
{
public:
    AudioGraph();
    ~AudioGraph();

    // --- Node management ---
    NodeId addNode(std::unique_ptr<AudioNode> node);
    void removeNode(NodeId id);
    AudioNode* getNode(NodeId id) const;

    // --- Connection management ---
    bool addConnection(const Connection& conn);
    void removeConnection(const Connection& conn);
    void disconnectNode(NodeId id);
    const std::vector<Connection>& getConnections() const;

    // --- I/O terminal node IDs ---
    NodeId getAudioInputNodeId() const  { return audioInputNodeId_; }
    NodeId getAudioOutputNodeId() const { return audioOutputNodeId_; }
    NodeId getMidiInputNodeId() const   { return midiInputNodeId_; }
    NodeId getMidiOutputNodeId() const  { return midiOutputNodeId_; }

    // --- Processing ---
    void prepare(double sampleRate, int maxBlockSize);
    void processBlock(AudioBlock& input, MidiBlock& midiIn,
                      AudioBlock& output, MidiBlock& midiOut,
                      int numSamples);
    void release();

    // --- Topology queries ---
    const std::vector<NodeId>& getProcessingOrder() const;
    bool wouldCreateCycle(const Connection& conn) const;

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
```

**Implementation notes:**

`AudioGraph()` constructor:
- Create 4 I/O terminal nodes. These are simple pass-through AudioNode subclasses
  (define them as private inner classes or in an anonymous namespace in the `.cpp`):
  - `AudioInputNode`: copies graph input audio to its output
  - `AudioOutputNode`: copies its input to graph output audio
  - `MidiInputNode`: copies graph input MIDI to its output
  - `MidiOutputNode`: copies its input to graph output MIDI
- Assign them sequential IDs (1-4), set `nextId_ = 5`

`addNode`: Insert into `nodes_`, increment `nextId_`, set `orderDirty_ = true`, return ID.

`removeNode`: Remove from `nodes_`, remove all connections involving this node,
set `orderDirty_ = true`.

`addConnection`:
- Validate both nodes exist
- Check `wouldCreateCycle` — return false if cycle detected
- Add to `connections_` vector
- Add to source node's `outputs` and dest node's `inputs`
- Set `orderDirty_ = true`

`removeConnection`: Remove from all vectors, set `orderDirty_ = true`.

`wouldCreateCycle`: Temporarily add the connection, run DFS from `conn.destNode`
looking for `conn.sourceNode`. Remove the temporary connection. Return true if found.

`rebuildProcessingOrder` — Kahn's algorithm:
1. Compute in-degree for each node (count of unique source nodes in `inputs`)
2. Enqueue nodes with in-degree 0
3. Process queue: dequeue, append to `processingOrder_`, decrement in-degree of successors
4. Assert `processingOrder_.size() == nodes_.size()` (cycle = bug)
5. Set `orderDirty_ = false`

`prepare`:
- Store `sampleRate_`, `maxBlockSize_`
- Call `prepare(sampleRate, maxBlockSize)` on every node
- Rebuild processing order
- Prepare buffer pool: `bufferPool_.prepare(nodes_.size() + 4, 2, maxBlockSize)`

`processBlock`:
- If `orderDirty_`, call `rebuildProcessingOrder()`
- Copy `input` into the audio input node's output buffer
- Copy `midiIn` into the MIDI input node's output MidiBlock
- Call `executor_.execute(processingOrder_, nodes_, bufferPool_, output, midiOut, numSamples)`

`release`: Call `release()` on every node.

`clear`: Remove all non-I/O nodes and all connections. Set `orderDirty_ = true`.

#### 7. `DelayNode.h` / `DelayNode.cpp`

Plugin Delay Compensation node. Inserted by the graph when parallel branches have
different latencies.

```cpp
#pragma once
#include "dc/engine/AudioNode.h"
#include <vector>

namespace dc {

class DelayNode : public AudioNode
{
public:
    void setDelay(int samples);
    int getDelay() const { return delaySamples_; }

    void prepare(double sampleRate, int maxBlockSize) override;
    void release() override;
    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override;
    std::string getName() const override { return "DelayNode"; }

private:
    int delaySamples_ = 0;
    int writePos_ = 0;
    std::vector<std::vector<float>> delayLines_;  // per-channel circular buffer
};
```

**Implementation:**
- `prepare`: Allocate `delayLines_` — one vector per channel, each of size `delaySamples_`
  (or 1 if delay is 0). Zero-fill. Reset `writePos_ = 0`.
- `process`: If `delaySamples_ == 0`, return immediately (pass-through).
  Otherwise, for each channel and each sample, read from `delayLines_[ch][(writePos_ + i) % size]`,
  write current sample into that position, output the read value.
- `setDelay`: Store the value. If already prepared, reallocate delay lines.
- MIDI: pass through unchanged (MIDI delay compensation is not implemented).

### CMakeLists.txt

Add the new source files to the `dc::engine` section in `target_sources(DremCanvas PRIVATE ...`.
Insert after the `dc::audio library` section:

```cmake
    # dc::engine library
    src/dc/engine/MidiBlock.cpp
    src/dc/engine/BufferPool.cpp
    src/dc/engine/GraphExecutor.cpp
    src/dc/engine/AudioGraph.cpp
    src/dc/engine/DelayNode.cpp
```

## Audio Thread Safety

- `BufferPool`, `GraphExecutor`, and `AudioGraph::processBlock` are called on the audio thread.
- **Zero allocations** in any audio-thread code path. All buffers come from `BufferPool`.
- `MidiBlock` wrapping an existing `MidiBuffer` is zero-alloc. Default-constructed
  `MidiBlock` owns a `MidiBuffer` which may allocate on first `addEvent` — this is
  acceptable during `prepare()` but not during `processBlock()`. In `GraphExecutor`,
  pre-allocate MidiBlocks during `prepare` or use the buffer pool pattern.
- Use `dc_assert` (from `dc/foundation/assert.h`) instead of `jassert`.
- Do NOT use `std::mutex`, `new`, `delete`, `malloc`, or any STL container `.push_back()`
  on the audio thread path. `std::vector` resizing happens only in `prepare()`.

## Scope Limitation

Do NOT touch any existing processor files (`TrackProcessor`, `MixBusProcessor`, etc.)
or `AudioEngine`. Those are handled by separate agents. Your scope is:
- `src/dc/audio/AudioBlock.h` (enhance)
- `src/dc/engine/*` (all new)
- `CMakeLists.txt` (add sources)

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
