# Agent: Engine Integration

You are working on the `feature/sans-juce-audio-graph` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 3 of the Sans-JUCE migration: rewrite `AudioEngine` to use `dc::AudioGraph`
instead of `juce::AudioProcessorGraph`, update `BounceProcessor`, delete `MidiBridge.h`,
and update `CMakeLists.txt`.

## Context

Read these specs before starting:
- `docs/sans-juce/02-audio-graph.md` (AudioGraph API, lifecycle, topology changes)
- `docs/sans-juce/08-migration-guide.md` (Phase 3 section, verification commands)
- `src/dc/engine/AudioGraph.h` (the dc::AudioGraph API you're wiring into)
- `src/dc/engine/AudioNode.h` (the dc::AudioNode interface)
- `src/dc/engine/MidiBlock.h` (dc::MidiBlock — wraps dc::MidiBuffer)
- `src/dc/audio/AudioBlock.h` (dc::AudioBlock — enhanced with addFrom/copyFrom/applyGain)
- `src/dc/audio/AudioDeviceManager.h` (dc::AudioDeviceManager — AudioCallback interface)
- `src/engine/AudioEngine.h` and `.cpp` (current implementation to rewrite)
- `src/engine/BounceProcessor.h` and `.cpp` (current implementation to rewrite)
- `src/engine/MidiBridge.h` (to be deleted)

Also read the migrated processor headers to understand their new interfaces:
- `src/engine/TrackProcessor.h` (now extends dc::AudioNode)
- `src/engine/MixBusProcessor.h` (now extends dc::AudioNode)
- `src/engine/MeterTapProcessor.h` (now extends dc::AudioNode)
- `src/engine/MetronomeProcessor.h` (now extends dc::AudioNode)
- `src/engine/MidiClipProcessor.h` (now extends dc::AudioNode)
- `src/engine/StepSequencerProcessor.h` (now extends dc::AudioNode)
- `src/engine/SimpleSynthProcessor.h` (now extends dc::AudioNode)

## Dependencies

This agent depends on ALL Tier 1 agents (01, 02, 03) being merged. All dc::engine
infrastructure and all processor migrations must be complete before running this agent.

## Deliverables

### 1. Rewrite `src/engine/AudioEngine.h`

Replace all `juce::AudioProcessorGraph` types with `dc::AudioGraph` equivalents.

```cpp
#pragma once
#include "dc/engine/AudioGraph.h"
#include "dc/audio/AudioDeviceManager.h"
#include <memory>
#include <string>

namespace dc {

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    void initialise(int numInputChannels, int numOutputChannels);
    void shutdown();

    dc::AudioGraph& getGraph() { return graph_; }

    // Node management — thin wrappers around dc::AudioGraph
    NodeId addProcessor(std::unique_ptr<AudioNode> processor);
    void removeProcessor(NodeId nodeId);
    void connectNodes(NodeId source, int sourceChannel,
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

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
};

} // namespace dc
```

**Key changes from current:**
- `std::unique_ptr<juce::AudioProcessorGraph> graph` → `dc::AudioGraph graph_` (value member, not pointer)
- `juce::AudioProcessorGraph::Node::Ptr` return types → `dc::NodeId` (uint32_t)
- `juce::AudioProcessorGraph::NodeID` parameters → `dc::NodeId`
- Remove `getAudioInputNode()` / `getAudioOutputNode()` returning `Node::Ptr` → replace with `getAudioInputNodeId()` / `getAudioOutputNodeId()` returning `NodeId`
- Remove `#include <JuceHeader.h>`
- `addProcessor` takes `std::unique_ptr<AudioNode>` instead of `std::unique_ptr<juce::AudioProcessor>`

### 2. Rewrite `src/engine/AudioEngine.cpp`

**GraphCallback class:**

The inner `GraphCallback` bridges `dc::AudioCallback` (from PortAudio) to `dc::AudioGraph::processBlock()`.

```cpp
class AudioEngine::GraphCallback : public dc::AudioCallback
{
public:
    explicit GraphCallback(dc::AudioGraph& graph)
        : graph_(graph)
    {
    }

    void audioDeviceAboutToStart(double sampleRate, int bufferSize) override
    {
        graph_.prepare(sampleRate, bufferSize);
    }

    void audioCallback(const float** inputChannelData, int numInputChannels,
                       float** outputChannelData, int numOutputChannels,
                       int numSamples) override
    {
        // Wrap input channels as AudioBlock (const_cast is safe — graph reads only)
        dc::AudioBlock inputBlock(const_cast<float**>(inputChannelData),
                                  numInputChannels, numSamples);

        // Wrap output channels as AudioBlock
        dc::AudioBlock outputBlock(outputChannelData, numOutputChannels, numSamples);
        outputBlock.clear();

        // Empty MIDI blocks for now (MIDI routing happens within the graph)
        dc::MidiBlock midiIn;
        dc::MidiBlock midiOut;

        graph_.processBlock(inputBlock, midiIn, outputBlock, midiOut, numSamples);
    }

    void audioDeviceStopped() override
    {
        graph_.release();
    }

private:
    dc::AudioGraph& graph_;
};
```

**Constructor / Destructor:**
```cpp
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}
```

**`initialise()`:**
```cpp
void AudioEngine::initialise(int numInputChannels, int numOutputChannels)
{
    deviceManager_ = dc::AudioDeviceManager::create();

    graphCallback_ = std::make_unique<GraphCallback>(graph_);
    deviceManager_->setCallback(graphCallback_.get());
    deviceManager_->openDefaultDevice(numInputChannels, numOutputChannels);

    double sampleRate = deviceManager_->isOpen() ? deviceManager_->getSampleRate() : 44100.0;
    int blockSize = deviceManager_->isOpen() ? deviceManager_->getBufferSize() : 512;

    graph_.prepare(sampleRate, blockSize);
}
```

No more I/O node creation — `AudioGraph` creates its own I/O terminal nodes in its constructor.

**`shutdown()`:**
```cpp
void AudioEngine::shutdown()
{
    if (deviceManager_)
    {
        deviceManager_->closeDevice();
        deviceManager_->setCallback(nullptr);
    }

    graphCallback_.reset();
    graph_.release();
    graph_.clear();
    deviceManager_.reset();
}
```

**`addProcessor()`:**
```cpp
NodeId AudioEngine::addProcessor(std::unique_ptr<AudioNode> processor)
{
    return graph_.addNode(std::move(processor));
}
```

**`removeProcessor()`:**
```cpp
void AudioEngine::removeProcessor(NodeId nodeId)
{
    graph_.removeNode(nodeId);
}
```

**`connectNodes()`:**
```cpp
void AudioEngine::connectNodes(NodeId source, int sourceChannel,
                                NodeId dest, int destChannel)
{
    dc::Connection conn{source, sourceChannel, dest, destChannel};
    bool ok = graph_.addConnection(conn);

    if (!ok)
    {
        auto* srcNode = graph_.getNode(source);
        auto* dstNode = graph_.getNode(dest);
        std::string srcName = srcNode ? srcNode->getName() : "?";
        std::string dstName = dstNode ? dstNode->getName() : "?";

        fprintf(stderr, "AudioEngine: FAILED connection %s[%d] -> %s[%d]\n",
                srcName.c_str(), sourceChannel,
                dstName.c_str(), destChannel);
        fflush(stderr);
    }
}
```

**Device info accessors:** unchanged (delegate to `deviceManager_`).

### 3. Rewrite `src/engine/BounceProcessor.h`

Replace `juce::AudioProcessorGraph&` parameter with `dc::AudioGraph&`.

```cpp
#pragma once
#include "dc/engine/AudioGraph.h"
#include <filesystem>
#include <functional>

namespace dc {

class BounceProcessor
{
public:
    BounceProcessor() = default;

    struct BounceSettings
    {
        std::filesystem::path outputFile;
        double sampleRate = 44100.0;
        int bitsPerSample = 24;
        int64_t startSample = 0;
        int64_t lengthInSamples = 0;
    };

    bool bounce(AudioGraph& graph, const BounceSettings& settings,
                std::function<void(float progress)> progressCallback = nullptr);

private:
    BounceProcessor(const BounceProcessor&) = delete;
    BounceProcessor& operator=(const BounceProcessor&) = delete;
};

} // namespace dc
```

### 4. Rewrite `src/engine/BounceProcessor.cpp`

Replace `juce::AudioProcessorGraph` API calls with `dc::AudioGraph` equivalents.

```cpp
#include "BounceProcessor.h"
#include "dc/audio/AudioFileWriter.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"
#include <vector>

namespace dc {

bool BounceProcessor::bounce(AudioGraph& graph, const BounceSettings& settings,
                              std::function<void(float progress)> progressCallback)
{
    if (settings.outputFile.empty() || settings.lengthInSamples <= 0)
        return false;

    std::filesystem::create_directories(settings.outputFile.parent_path());

    if (std::filesystem::exists(settings.outputFile))
        std::filesystem::remove(settings.outputFile);

    const int numChannels = 2;  // stereo output

    AudioFileWriter::Format format;
    switch (settings.bitsPerSample)
    {
        case 16: format = AudioFileWriter::Format::WAV_16; break;
        case 32: format = AudioFileWriter::Format::WAV_32F; break;
        default: format = AudioFileWriter::Format::WAV_24; break;
    }

    auto writer = AudioFileWriter::create(settings.outputFile, format,
                                           numChannels, settings.sampleRate);
    if (writer == nullptr)
        return false;

    constexpr int blockSize = 512;

    // Prepare the graph for offline rendering
    graph.prepare(settings.sampleRate, blockSize);

    // Allocate channel data
    std::vector<float> channelStorage(static_cast<size_t>(numChannels * blockSize), 0.0f);
    std::vector<float*> channelPtrs(static_cast<size_t>(numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        channelPtrs[static_cast<size_t>(ch)] = channelStorage.data() + ch * blockSize;

    // Empty input (offline bounce has no live input)
    std::vector<float> emptyStorage(static_cast<size_t>(numChannels * blockSize), 0.0f);
    std::vector<float*> emptyPtrs(static_cast<size_t>(numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        emptyPtrs[static_cast<size_t>(ch)] = emptyStorage.data() + ch * blockSize;

    int64_t samplesRemaining = settings.lengthInSamples;
    int64_t samplesProcessed = 0;

    while (samplesRemaining > 0)
    {
        const int samplesToProcess = static_cast<int>(
            std::min(static_cast<int64_t>(blockSize), samplesRemaining));

        dc::AudioBlock inputBlock(emptyPtrs.data(), numChannels, samplesToProcess);
        inputBlock.clear();

        dc::AudioBlock outputBlock(channelPtrs.data(), numChannels, samplesToProcess);
        outputBlock.clear();

        dc::MidiBlock midiIn;
        dc::MidiBlock midiOut;

        graph.processBlock(inputBlock, midiIn, outputBlock, midiOut, samplesToProcess);

        writer->write(outputBlock, samplesToProcess);

        samplesProcessed += samplesToProcess;
        samplesRemaining -= samplesToProcess;

        if (progressCallback != nullptr)
        {
            float progress = static_cast<float>(samplesProcessed)
                           / static_cast<float>(settings.lengthInSamples);
            progressCallback(progress);
        }
    }

    writer->close();
    graph.release();

    return true;
}

} // namespace dc
```

### 5. Delete `src/engine/MidiBridge.h`

This file is no longer referenced. Delete it entirely.

### 6. Update callers of AudioEngine API

Search the codebase for all callers of the old AudioEngine API and update them.
The main changes callers need:

| Old API | New API |
|---------|---------|
| `engine.addProcessor(std::make_unique<TrackProcessor>(...))` returns `Node::Ptr` | Returns `dc::NodeId` (uint32_t) |
| `node->nodeID` to get ID from Node::Ptr | Already have `NodeId` from `addProcessor()` |
| `engine.getAudioInputNode()->nodeID` | `engine.getAudioInputNodeId()` |
| `engine.getAudioOutputNode()->nodeID` | `engine.getAudioOutputNodeId()` |
| `engine.getGraph()` returns `juce::AudioProcessorGraph&` | Returns `dc::AudioGraph&` |
| `engine.connectNodes(nodeId, ch, ...)` with `juce::AudioProcessorGraph::NodeID` | Same call with `dc::NodeId` (uint32_t) |

Search for these patterns to find all callers:
```
getAudioInputNode
getAudioOutputNode
addProcessor
removeProcessor
connectNodes
->nodeID
```

Typical caller code (e.g., in `Project.cpp` or `AppController.cpp`) changes from:
```cpp
auto node = engine.addProcessor(std::make_unique<TrackProcessor>(transport));
auto nodeId = node->nodeID;
engine.connectNodes(engine.getAudioInputNode()->nodeID, 0, nodeId, 0);
```
to:
```cpp
auto nodeId = engine.addProcessor(std::make_unique<TrackProcessor>(transport));
engine.connectNodes(engine.getAudioInputNodeId(), 0, nodeId, 0);
```

### 7. Update `CMakeLists.txt`

Ensure all `dc::engine` source files are in `target_sources`. Add after the
`# dc::audio library` section (if not already added by Agent 07):

```cmake
    # dc::engine library
    src/dc/engine/MidiBlock.cpp
    src/dc/engine/BufferPool.cpp
    src/dc/engine/GraphExecutor.cpp
    src/dc/engine/AudioGraph.cpp
    src/dc/engine/DelayNode.cpp
```

Remove `MidiBridge.h` — it's header-only so it won't be in `target_sources`,
but verify no stale references exist in CMake.

## Verification

After all changes, run these verification commands:

```bash
# 1. Build
cmake --build --preset release

# 2. Grep check — zero hits for juce::AudioProcessor (excluding plugin types)
grep -rn "juce::AudioProcessor\|juce::AudioProcessorGraph\|AudioProcessorPlayer" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm" | \
    grep -v "juce::AudioPluginInstance\|juce::AudioProcessorEditor"

# 3. Verify MidiBridge.h is deleted
test ! -f src/engine/MidiBridge.h && echo "OK: MidiBridge.h deleted"

# 4. Run the app (Linux)
./build/DremCanvas_artefacts/Release/DremCanvas
```

The grep in step 2 should return zero hits. All `juce::AudioProcessor` references
should be gone from `src/engine/`. The only remaining JUCE AudioProcessor references
should be in `src/plugins/` (Phase 4 scope: `juce::AudioPluginInstance`,
`juce::AudioProcessorEditor`).

## Scope Limitation

- Do NOT modify any processor files (`TrackProcessor`, `MixBusProcessor`, etc.) —
  those are already migrated by Agents 02 and 03.
- Do NOT modify plugin files (`src/plugins/*`) — those stay on JUCE until Phase 4.
- Do NOT modify `dc::engine` infrastructure files (`AudioGraph.h`, `BufferPool.h`, etc.) —
  those are created by Agent 01. If you find a bug, work around it and leave a `// TODO:`
  comment.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
