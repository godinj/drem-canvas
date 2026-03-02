# Agent: Thread-Safe Audio Graph Topology Mutations

You are working on the `feature/plugin-loading-fixes` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is adding a `suspendProcessing` mechanism to
`dc::AudioGraph` so that topology mutations (add/remove node, add/remove connection)
don't race with the audio thread.

## Context

Read these before starting:
- `src/dc/engine/AudioGraph.h` (full file — `addNode`, `removeNode`, `addConnection`, `removeConnection`, `processBlock`)
- `src/dc/engine/AudioGraph.cpp` (full file — current implementation, `rebuildProcessingOrder`)
- `src/dc/engine/GraphExecutor.h` / `.cpp` (the executor that walks `processingOrder_`)
- `src/engine/AudioEngine.h` / `.cpp` (`addProcessor`, `removeProcessor`, `GraphCallback::audioCallback`)
- `src/ui/AppController.cpp` (search for `TODO: add suspendProcessing` — at least two locations: `rebuildAudioGraph` ~line 1338 and `insertPluginOnTrack` ~line 1850)
- `src/dc/foundation/spsc_queue.h` (lock-free SPSC queue — available for audio-thread communication)

## Problem

`AudioGraph::addNode()`, `removeNode()`, `addConnection()`, `removeConnection()` mutate
`nodes_`, `connections_`, and `processingOrder_` — the same data structures that the audio
thread reads in `processBlock()` → `GraphExecutor::execute()`. There is no synchronization.

Currently the code has two `TODO` comments:
```cpp
// TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
```

The most dangerous path is `insertPluginOnTrack` (line 1841) where `createPluginAsync`'s
callback runs on a background thread and calls `disconnectTrackPluginChain` /
`audioEngine.addProcessor` / `connectTrackPluginChain` while the audio thread is processing.

## Design Approach

Use a **swap-on-commit** strategy (double-buffered graph state):

1. **Suspend**: Atomically flag the graph as "suspended". While suspended, `processBlock()`
   outputs silence (or processes the last-known-good topology).
2. **Mutate**: Caller makes topology changes on the message thread.
3. **Resume**: Atomically swap in the new topology. Audio thread picks it up on next callback.

This avoids locks on the audio thread entirely.

### Alternative (simpler): spin-lock with tryLock

A simpler approach that's acceptable for this project's scale:

1. Add `std::atomic<bool> suspended_ {false}` to `AudioGraph`.
2. `suspendProcessing()` sets `suspended_ = true`.
3. `resumeProcessing()` rebuilds processing order and sets `suspended_ = false`.
4. `processBlock()` checks `suspended_` at the top — if true, output silence and return.
5. All topology mutations assert `suspended_ == true`.

This works because topology mutations are infrequent (user action) and the suspend window
is short (microseconds). The audio thread sees at most one silent buffer during the swap.

Choose whichever approach you judge best. The simpler approach is preferred unless you
see a specific problem with it.

## Deliverables

### Modified files

#### 1. `src/dc/engine/AudioGraph.h`

Add suspend/resume API:

```cpp
/// Suspend audio processing. While suspended, processBlock() outputs silence.
/// Must be called from the message thread before topology mutations.
void suspendProcessing();

/// Resume audio processing after topology mutations.
/// Rebuilds processing order before resuming.
void resumeProcessing();

/// Check if processing is currently suspended.
bool isSuspended() const;
```

Add the atomic flag:

```cpp
std::atomic<bool> suspended_ {false};
```

#### 2. `src/dc/engine/AudioGraph.cpp`

Implement suspend/resume:

```cpp
void AudioGraph::suspendProcessing()
{
    suspended_.store (true);
}

void AudioGraph::resumeProcessing()
{
    rebuildProcessingOrder();
    suspended_.store (false);
}

bool AudioGraph::isSuspended() const
{
    return suspended_.load();
}
```

In `processBlock()`, add an early-out at the top:

```cpp
void AudioGraph::processBlock (AudioBlock& input, MidiBlock& midiIn,
                               AudioBlock& output, MidiBlock& midiOut,
                               int numSamples)
{
    if (suspended_.load (std::memory_order_acquire))
    {
        output.clear();
        midiOut.clear();
        return;
    }

    // ... existing processing code ...
}
```

In `addNode()`, `removeNode()`, `addConnection()`, `removeConnection()`, add:
```cpp
dc_assert (suspended_.load() && "Topology mutations require suspended processing");
```

Wait — this assertion would break the existing `rebuildAudioGraph()` flow which doesn't
suspend yet. Instead, add the assertion only after you've updated all callers. Or make
the assertion a warning log rather than a hard assert, to be tightened later.

#### 3. `src/engine/AudioEngine.h` / `.cpp`

Add convenience wrappers:

```cpp
void AudioEngine::suspendProcessing()
{
    graph_.suspendProcessing();
}

void AudioEngine::resumeProcessing()
{
    graph_.resumeProcessing();
}
```

#### 4. `src/ui/AppController.cpp`

Replace the two `TODO` comments with actual suspend/resume calls.

**In `rebuildAudioGraph()` (~line 1338)**:
```cpp
void AppController::rebuildAudioGraph()
{
    audioEngine.suspendProcessing();

    // ... existing topology mutations ...

    audioEngine.resumeProcessing();
}
```

**In `insertPluginOnTrack()` (~line 1850)**:
```cpp
pluginHost.createPluginAsync (desc, sampleRate, blockSize,
    [this, trackIndex] (std::unique_ptr<dc::PluginInstance> instance, ...)
    {
        if (instance == nullptr) return;

        audioEngine.suspendProcessing();

        // ... existing disconnect/add/connect ...

        audioEngine.resumeProcessing();
    });
```

Search for ALL other topology mutation call sites in `AppController.cpp` (grep for
`addProcessor`, `removeProcessor`, `connectNodes`, `disconnectNode`,
`disconnectTrackPluginChain`, `connectTrackPluginChain`) and wrap them with
suspend/resume pairs.

#### 5. `tests/unit/engine/test_audio_graph_suspend.cpp` (NEW)

Test the suspend mechanism:

- `processBlock` outputs silence when suspended
- `processBlock` produces output when resumed
- Topology mutations during suspend don't crash
- `resumeProcessing` rebuilds processing order
- Rapid suspend/resume cycles don't deadlock or crash

### Modified files

#### 6. `tests/CMakeLists.txt`

Add `tests/unit/engine/test_audio_graph_suspend.cpp` to the test target.

## Audio Thread Safety

- `suspended_` is `std::atomic<bool>` — lock-free read from audio thread
- No mutexes on the audio thread path
- Topology mutations happen on message thread only (assert this if possible)
- The audio thread sees at most one silent buffer during suspend window
- `rebuildProcessingOrder()` is called in `resumeProcessing()` before the atomic store,
  so the audio thread always sees a consistent processing order

## Scope Limitation

Do NOT modify `PluginInstance`, `ProbeCache`, `VST3Host`, or `VST3Module`.
Focus on `AudioGraph`, `AudioEngine`, and `AppController` topology call sites.

Do NOT add lock-free queues or double-buffered graph state unless the simple atomic
approach proves insufficient. Start simple.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Run verification: `scripts/verify.sh`
