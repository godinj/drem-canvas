# Agent: PluginInstance + ComponentHandler

You are working on the `feature/sans-juce-plugin-hosting` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 (VST3 Plugin Hosting): implement `dc::PluginInstance` (wraps a loaded
VST3 component, implements `dc::AudioNode`) and `dc::ComponentHandler` (host-side
`IComponentHandler` receiving parameter edits from the plugin).

## Context

Read these specs before starting:
- `docs/sans-juce/03-plugin-hosting.md` (PluginInstance, IComponentHandler sections)
- `docs/sans-juce/02-audio-graph.md` (AudioNode interface, AudioBlock, MidiBlock)
- `docs/sans-juce/08-migration-guide.md` (Phase 4 — new files to create)
- `src/dc/plugins/VST3Module.h` (module loading — created by Agent 01)
- `src/dc/plugins/PluginDescription.h` (metadata — created by Agent 01)
- `src/dc/audio/AudioBlock.h` (existing AudioBlock implementation)
- `src/dc/midi/MidiBuffer.h` (existing MidiBuffer implementation)
- `src/dc/foundation/spsc_queue.h` (lock-free SPSC queue for edit events)

## Dependencies

This agent depends on Agent 01 (VST3Module, PluginDescription). If those files don't
exist yet, create stub headers matching the interfaces in `docs/sans-juce/03-plugin-hosting.md`.

**Phase 3 stubs**: `dc::AudioNode` and `dc::MidiBlock` do not exist yet (they are Phase 3
deliverables). Create stub headers for them so `PluginInstance` can compile:

- `src/dc/engine/AudioNode.h` — the interface from `docs/sans-juce/02-audio-graph.md`
- `src/dc/engine/MidiBlock.h` — non-owning wrapper around `dc::MidiBuffer`

These stubs will be replaced by the full Phase 3 implementation later.

## Deliverables

### New files (src/dc/engine/) — Phase 3 stubs

#### 1. AudioNode.h (stub)

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
    virtual void process(AudioBlock& audio, MidiBlock& midi,
                         int numSamples) = 0;

    virtual int getLatencySamples() const { return 0; }
    virtual int getNumInputChannels() const { return 2; }
    virtual int getNumOutputChannels() const { return 2; }
    virtual bool acceptsMidi() const { return false; }
    virtual bool producesMidi() const { return false; }
    virtual std::string getName() const { return "AudioNode"; }
};

} // namespace dc
```

#### 2. MidiBlock.h (stub)

```cpp
#pragma once
#include "dc/midi/MidiBuffer.h"
#include "dc/midi/MidiMessage.h"

namespace dc {

class MidiBlock
{
public:
    MidiBlock() = default;
    explicit MidiBlock(MidiBuffer& buffer) : buffer_(&buffer) {}

    MidiBuffer::Iterator begin() const { return buffer_ ? buffer_->begin() : MidiBuffer::Iterator(); }
    MidiBuffer::Iterator end() const { return buffer_ ? buffer_->end() : MidiBuffer::Iterator(); }

    void addEvent(const MidiMessage& msg, int sampleOffset)
    {
        if (buffer_) buffer_->addEvent(msg, sampleOffset);
    }

    void clear() { if (buffer_) buffer_->clear(); }
    int getNumEvents() const { return buffer_ ? buffer_->getNumEvents() : 0; }
    bool isEmpty() const { return !buffer_ || buffer_->isEmpty(); }

private:
    MidiBuffer* buffer_ = nullptr;
};

} // namespace dc
```

**Note**: Check `dc::MidiBuffer::Iterator` — if it has no default constructor, add one
or adjust the stub accordingly. The MidiBlock stub must compile against the existing
`dc::MidiBuffer` in `src/dc/midi/MidiBuffer.h`.

### New files (src/dc/plugins/)

#### 3. ComponentHandler.h

Host-side `IComponentHandler` implementation. The plugin calls these methods when
the user interacts with its UI (twists a knob, etc.).

```cpp
#pragma once
#include "dc/foundation/spsc_queue.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <functional>
#include <atomic>

namespace dc {

struct EditEvent
{
    Steinberg::Vst::ParamID paramId;
    double value;
};

class ComponentHandler
    : public Steinberg::Vst::IComponentHandler
{
public:
    explicit ComponentHandler(SPSCQueue<EditEvent>& editQueue);

    // --- IComponentHandler ---
    Steinberg::tresult PLUGIN_API beginEdit(
        Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API performEdit(
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::ParamValue valueNormalized) override;
    Steinberg::tresult PLUGIN_API endEdit(
        Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API restartComponent(
        Steinberg::int32 flags) override;

    // --- FUnknown ---
    Steinberg::tresult PLUGIN_API queryInterface(
        const Steinberg::TUID iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // --- Extensions ---

    /// Callback for parameter changes (for automation recording).
    /// Called from the plugin's UI thread during performEdit.
    void setParameterChangeCallback(
        std::function<void(Steinberg::Vst::ParamID, double)> cb);

    /// Callback for restartComponent (latency change, etc.)
    void setRestartCallback(
        std::function<void(Steinberg::int32)> cb);

private:
    SPSCQueue<EditEvent>& editQueue_;
    std::function<void(Steinberg::Vst::ParamID, double)> paramCallback_;
    std::function<void(Steinberg::int32)> restartCallback_;
    std::atomic<Steinberg::uint32> refCount_{1};
};

} // namespace dc
```

**Implementation notes**:
- `performEdit()`: Push `{id, valueNormalized}` to `editQueue_`, call `paramCallback_`
  if set, return `kResultOk`
- `beginEdit()` / `endEdit()`: Return `kResultOk` (no-op for now; can add
  undo transaction grouping later)
- `restartComponent()`: Call `restartCallback_` if set, return `kResultOk`
- `queryInterface()`: Return `kResultOk` for `IComponentHandler` IID, `kNoInterface` otherwise
- `addRef()` / `release()`: Standard ref-counting. Do NOT `delete this` on release —
  the handler is owned by `PluginInstance` (prevent double-free)

#### 4. ComponentHandler.cpp

Implement the methods described above.

#### 5. PluginInstance.h

Wraps a loaded VST3 component. Implements `dc::AudioNode` for audio graph integration.

```cpp
#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/plugins/PluginDescription.h"
#include "dc/plugins/ComponentHandler.h"
#include "dc/foundation/spsc_queue.h"
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstparameterfinderext.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dc {

class VST3Module;
class PluginEditor;

class PluginInstance : public AudioNode
{
public:
    /// Create from a loaded module and description.
    /// Returns nullptr on failure.
    static std::unique_ptr<PluginInstance> create(
        VST3Module& module,
        const PluginDescription& desc,
        double sampleRate,
        int maxBlockSize);

    ~PluginInstance() override;

    PluginInstance(const PluginInstance&) = delete;
    PluginInstance& operator=(const PluginInstance&) = delete;

    // --- AudioNode interface ---
    void prepare(double sampleRate, int maxBlockSize) override;
    void release() override;
    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override;
    int getLatencySamples() const override;
    int getNumInputChannels() const override;
    int getNumOutputChannels() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    std::string getName() const override;

    // --- Parameters ---
    int getNumParameters() const;
    std::string getParameterName(int index) const;
    std::string getParameterLabel(int index) const;
    float getParameterValue(int index) const;       // 0.0-1.0 normalized
    void setParameterValue(int index, float value);
    Steinberg::Vst::ParamID getParameterId(int index) const;
    std::string getParameterDisplay(int index) const;

    // --- State ---
    std::vector<uint8_t> getState() const;
    void setState(const std::vector<uint8_t>& data);

    // --- Editor ---
    bool hasEditor() const;
    std::unique_ptr<PluginEditor> createEditor();

    // --- IParameterFinder (spatial hints) ---
    bool supportsParameterFinder() const;
    int findParameterAtPoint(int x, int y) const;

    // --- performEdit snoop ---
    std::optional<EditEvent> popLastEdit();

    // --- Description ---
    const PluginDescription& getDescription() const;

    // --- Internal accessors (for PluginEditor) ---
    Steinberg::Vst::IEditController* getController() const;

private:
    PluginInstance() = default;

    // VST3 interfaces (prevent release in wrong order)
    Steinberg::Vst::IComponent* component_ = nullptr;
    Steinberg::Vst::IAudioProcessor* processor_ = nullptr;
    Steinberg::Vst::IEditController* controller_ = nullptr;
    Steinberg::Vst::IParameterFinder* parameterFinder_ = nullptr;
    std::unique_ptr<ComponentHandler> handler_;
    PluginDescription description_;

    // Audio processing state
    Steinberg::Vst::ProcessData processData_{};
    Steinberg::Vst::AudioBusBuffers inputBusBuffers_{};
    Steinberg::Vst::AudioBusBuffers outputBusBuffers_{};

    // MIDI event conversion buffer
    std::vector<Steinberg::Vst::Event> eventBuffer_;
    Steinberg::Vst::EventList* eventList_ = nullptr;

    // Parameter cache
    struct ParamInfo
    {
        Steinberg::Vst::ParamID id;
        std::string name;
        std::string label;
    };
    std::vector<ParamInfo> parameters_;

    // performEdit snoop queue
    SPSCQueue<EditEvent> editEvents_{64};

    // State
    double currentSampleRate_ = 44100.0;
    int currentBlockSize_ = 512;
    bool prepared_ = false;

    // Internal helpers
    void buildParameterList();
    int getParameterIndex(Steinberg::Vst::ParamID id) const;
    void setupProcessing(double sampleRate, int maxBlockSize);
    void connectControllerToComponent();
};

} // namespace dc
```

#### 6. PluginInstance.cpp

Implementation of the VST3 component wrapper. This is the most complex file.

**`create(module, desc, sampleRate, maxBlockSize)`:**
1. Get factory from module
2. Create `IComponent` via `factory->createInstance(uid, IComponent::iid, ...)`
3. Initialize component: `component->initialize(hostContext)` — for `hostContext`,
   pass nullptr or a minimal `IHostApplication` implementation
4. Query `IAudioProcessor` from component (`queryInterface`)
5. Get controller class ID: `component->getControllerClassId(controllerId)`
6. Create `IEditController` from factory using the controller class ID
   - If the component itself IS the controller (same class, implements both), use
     `queryInterface` on the component instead
7. Initialize controller: `controller->initialize(hostContext)`
8. Create `ComponentHandler`, set it on the controller:
   `controller->setComponentHandler(handler.get())`
9. Connect controller ↔ component if they implement `IConnectionPoint`
10. Query `IParameterFinder` from controller (`queryInterface`)
11. Call `setupProcessing()` and `prepare()`
12. Build parameter list

**`setupProcessing(sampleRate, maxBlockSize)`:**
- Fill `Steinberg::Vst::ProcessSetup` struct
- Call `processor->setupProcessing(setup)`
- Set `component->setActive(true)`
- Call `processor->setProcessing(true)`

**`process(audio, midi, numSamples)`:**
1. Set up `processData_`:
   - `processData_.numSamples = numSamples`
   - `processData_.numInputs = 1; processData_.numOutputs = 1`
   - Point `inputBusBuffers_.channelBuffers32` to `audio.getChannel()` pointers
   - Point `outputBusBuffers_.channelBuffers32` to audio output channel pointers
   - `processData_.inputs = &inputBusBuffers_`
   - `processData_.outputs = &outputBusBuffers_`
2. Convert `MidiBlock` events to `Steinberg::Vst::Event` array:
   - For each event in midi: create `Event` with `Event::kNoteOnEvent` /
     `Event::kNoteOffEvent` / `Event::kDataEvent` as appropriate
   - Attach event list to `processData_.inputEvents`
3. Call `processor->process(processData_)`
4. Copy output events back to MidiBlock if plugin produces MIDI
5. Note: for effects (not synths), input and output may alias the same buffers
   (in-place processing). Check `kIsPlaceable` flag or use separate buffers.

**`getState()` / `setState()`:**
- Use `Steinberg::IBStream` (or a custom implementation wrapping `std::vector<uint8_t>`)
- `component->getState(stream)` — writes component state
- `controller->getState(stream)` — appends controller state (if separate)
- Combine both into the returned vector
- For `setState`: split the data and call `component->setState()` then
  `controller->setComponentState()` and `controller->setState()`

**`buildParameterList()`:**
- `controller->getParameterCount()`
- For each parameter: `controller->getParameterInfo(i, info)`
- Store `ParamInfo{info.id, toString(info.title), toString(info.units)}`
- Convert `Steinberg::Vst::String128` to `std::string` using UTF-16 to UTF-8 conversion

**`getParameterValue(index)` / `setParameterValue(index, value)`:**
- `controller->getParamNormalized(parameters_[index].id)`
- `controller->setParamNormalized(parameters_[index].id, value)`
- For `setParameterValue`, also call `handler->performEdit` to notify automation

**`findParameterAtPoint(x, y)`:**
- If `parameterFinder_` is null, return -1
- Call `parameterFinder_->findParameter(x, y, resultId)`
- Convert `resultId` to parameter index via `getParameterIndex()`

**`popLastEdit()`:**
- Dequeue from `editEvents_` SPSC queue
- Return the event or `nullopt`

**Important VST3 SDK types/headers:**
- `pluginterfaces/vst/ivstaudioprocessor.h` — `IAudioProcessor`, `ProcessData`, `ProcessSetup`
- `pluginterfaces/vst/ivstcomponent.h` — `IComponent`
- `pluginterfaces/vst/ivsteditcontroller.h` — `IEditController`, `ParameterInfo`
- `pluginterfaces/vst/ivstparameterfinderext.h` — `IParameterFinder` (may be in
  `pluginterfaces/vst/ivsteditcontroller.h` depending on SDK version)
- `pluginterfaces/vst/ivstevents.h` — `Event`, `IEventList`
- `pluginterfaces/base/ibstream.h` — `IBStream`
- `pluginterfaces/base/funknown.h` — `FUnknownPtr`, FUID
- `pluginterfaces/vst/ivstmidicontrollers.h` — MIDI CC mapping constants
- Use `FUnknownPtr<T>(obj)` for safe `queryInterface` wrapper

### Modified files

#### 7. CMakeLists.txt

Add to `target_sources` under `# dc::plugins library`:
```cmake
src/dc/plugins/ComponentHandler.cpp
src/dc/plugins/PluginInstance.cpp
```

## Audio Thread Safety

- `process()` runs on the audio thread — no allocations, no locks, no blocking calls
- The `eventBuffer_` vector must be pre-allocated in `prepare()` to avoid reallocation
  during `process()`
- `editEvents_` uses `dc::SPSCQueue` which is lock-free

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Use `dc_log()` from `dc/foundation/assert.h` for logging
- Use `dc_assert()` for precondition checks
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
