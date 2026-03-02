# Agent: MIDI Processor Migration

You are working on the `feature/sans-juce-audio-graph` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 3 of the Sans-JUCE migration: migrate the 3 MIDI-aware processor classes
from `juce::AudioProcessor` to `dc::AudioNode` and eliminate all `bridge::toJuce`/`fromJuce`
calls.

## Context

Read these specs before starting:
- `docs/sans-juce/02-audio-graph.md` (AudioNode interface, migration table)
- `docs/sans-juce/08-migration-guide.md` (Phase 3 section)
- `src/engine/MidiClipProcessor.h` and `.cpp` (current implementation)
- `src/engine/StepSequencerProcessor.h` and `.cpp` (current implementation)
- `src/engine/SimpleSynthProcessor.h` (current implementation — header-only)
- `src/engine/MidiBridge.h` (bridge being eliminated — read to understand conversions)
- `src/dc/midi/MidiBuffer.h` (dc::MidiBuffer — the type you'll use directly)
- `src/dc/midi/MidiMessage.h` (dc::MidiMessage — already used internally)

## Dependencies

This agent depends on Agent 01 (Graph Infrastructure) for `dc::AudioNode` and `dc::MidiBlock`.
If `src/dc/engine/AudioNode.h` does not exist, create this stub so your code compiles:

```cpp
// src/dc/engine/AudioNode.h — stub (replaced by Agent 01)
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

Similarly, if `src/dc/engine/MidiBlock.h` does not exist, create this stub:

```cpp
// src/dc/engine/MidiBlock.h — stub (replaced by Agent 01)
#pragma once
#include "dc/midi/MidiBuffer.h"

namespace dc {
class MidiBlock
{
public:
    MidiBlock() : buffer_(&ownedBuffer_) {}
    explicit MidiBlock(MidiBuffer& buffer) : buffer_(&buffer) {}
    MidiBuffer::Iterator begin() const { return buffer_->begin(); }
    MidiBuffer::Iterator end() const { return buffer_->end(); }
    void addEvent(const MidiMessage& msg, int sampleOffset) { buffer_->addEvent(msg, sampleOffset); }
    void clear() { buffer_->clear(); }
    int getNumEvents() const { return buffer_->getNumEvents(); }
    bool isEmpty() const { return buffer_->isEmpty(); }
private:
    MidiBuffer* buffer_ = nullptr;
    MidiBuffer ownedBuffer_;
};
} // namespace dc
```

## Deliverables

### Migration pattern (applied to all 3 processors)

Same base transformation as the audio processors (see Agent 02), plus MIDI-specific changes:

**Remove:**
- `#include <JuceHeader.h>`
- `#include "MidiBridge.h"`
- `: public juce::AudioProcessor` base class
- `BusesProperties()` constructor delegation
- All JUCE boilerplate overrides (getName returning juce::String, programs, state, editor)
- All `bridge::toJuce(...)` calls
- All `bridge::fromJuce(...)` calls

**Replace:**
- `: public juce::AudioProcessor` → `: public dc::AudioNode`
- `#include <JuceHeader.h>` → `#include "dc/engine/AudioNode.h"` and `#include "dc/engine/MidiBlock.h"`
- `processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)` → `process(AudioBlock& audio, MidiBlock& midi, int numSamples)`

**MIDI-specific changes:**
- The `midiMessages` parameter was `juce::MidiBuffer&`. Now `midi` is `dc::MidiBlock&`.
- Processors that accumulated events in a local `dc::MidiBuffer dcMidi` and then called
  `bridge::toJuce(dcMidi, midiMessages)` at the end: **write directly to the `midi` parameter
  instead**. Remove the local `dcMidi` variable and the `bridge::toJuce` call.
- Processors that called `bridge::fromJuce(midiMessages)` to read input MIDI: **read
  from the `midi` parameter directly** using its `begin()`/`end()` iterators.

### 1. `src/engine/MidiClipProcessor.h`

After migration: inherits `dc::AudioNode`, stereo output, accepts + produces MIDI.

```cpp
#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/midi/MidiMessage.h"
#include "dc/midi/MidiBuffer.h"
#include "dc/audio/AudioBlock.h"
#include "dc/foundation/spsc_queue.h"
#include <atomic>
#include <array>

namespace dc {

class MidiClipProcessor : public AudioNode
{
public:
    // ... MidiNoteEvent, MidiTrackSnapshot structs unchanged ...

    explicit MidiClipProcessor(TransportController& transport);

    // AudioNode interface
    void prepare(double sampleRate, int maxBlockSize) override;
    void release() override {}
    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "MidiClipProcessor"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }

    // ... rest of public API unchanged (updateSnapshot, injectLiveMidi, etc.) ...

private:
    // ... private members unchanged ...

    // Change: drainLiveMidiFifo now writes to MidiBlock instead of dc::MidiBuffer
    void drainLiveMidiFifo(MidiBlock& midi);
    void processNoteOffs(MidiBlock& midi, int64_t blockStart, int numSamples);
};

} // namespace dc
```

### 2. `src/engine/MidiClipProcessor.cpp`

Key changes:

**Constructor:** Remove `AudioProcessor(BusesProperties()...)`. Just:
```cpp
MidiClipProcessor::MidiClipProcessor(TransportController& transport)
    : transportController(transport)
{
}
```

**Remove:** `#include "MidiBridge.h"`

**`process()` method:**
- Signature: `void MidiClipProcessor::process(AudioBlock& audio, MidiBlock& midi, int numSamples)`
- Remove `dc::AudioBlock block(...)` wrapper — use `audio` directly
- Remove `dc::MidiBuffer dcMidi;` local variable
- Replace all `dcMidi.addEvent(...)` → `midi.addEvent(...)`
- Remove `bridge::toJuce(dcMidi, midiMessages)` at every return point
- Replace `drainLiveMidiFifo(dcMidi)` → `drainLiveMidiFifo(midi)`
- Replace `processNoteOffs(dcMidi, ...)` → `processNoteOffs(midi, ...)`

**`drainLiveMidiFifo()` method:**
- Change parameter: `dc::MidiBuffer& dcMidi` → `MidiBlock& midi`
- Change body: `dcMidi.addEvent(msg, 0)` → `midi.addEvent(msg, 0)`

**`processNoteOffs()` method:**
- Change parameter: `dc::MidiBuffer& dcMidi` → `MidiBlock& midi`
- Change body: `dcMidi.addEvent(...)` → `midi.addEvent(...)`

### 3. `src/engine/StepSequencerProcessor.h`

After migration: inherits `dc::AudioNode`, stereo output, produces MIDI.

```cpp
// Key interface changes:
std::string getName() const override { return "StepSequencer"; }
int getNumInputChannels() const override { return 0; }
int getNumOutputChannels() const override { return 2; }
bool acceptsMidi() const override { return false; }
bool producesMidi() const override { return true; }
```

Change private method signatures:
- `void processNoteOffs(MidiBlock& midi, int64_t blockStart, int numSamples);`

### 4. `src/engine/StepSequencerProcessor.cpp`

Same pattern as MidiClipProcessor:

**Constructor:** Remove `AudioProcessor(BusesProperties()...)`.

**Remove:** `#include "MidiBridge.h"`

**`process()` method:**
- Remove `dc::MidiBuffer dcMidi;` local variable
- Replace all `dcMidi.addEvent(...)` → `midi.addEvent(...)`
- Remove all `bridge::toJuce(dcMidi, midiMessages)` calls (there are 2: one early return, one at end)
- Replace `processNoteOffs(dcMidi, ...)` → `processNoteOffs(midi, ...)`

**`processNoteOffs()` method:**
- Change parameter and body same as MidiClipProcessor

### 5. `src/engine/SimpleSynthProcessor.h`

This is a header-only file. After migration:

```cpp
#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "dc/audio/AudioBlock.h"
#include "dc/midi/MidiMessage.h"
#include "dc/foundation/types.h"
#include <array>
#include <cmath>

namespace dc {

class SimpleSynthProcessor : public AudioNode
{
public:
    SimpleSynthProcessor() = default;

    std::string getName() const override { return "SimpleSynth"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }

    void prepare(double sampleRate, int /*maxBlockSize*/) override
    {
        currentSampleRate = sampleRate;
        for (auto& v : voices)
            v = {};
    }

    void release() override {}

    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override
    {
        audio.clear();

        // Read MIDI directly from the MidiBlock parameter (no bridge conversion)
        for (auto it = midi.begin(); it != midi.end(); ++it)
        {
            auto event = *it;
            const auto& msg = event.message;

            if (msg.isNoteOn())
                noteOn(msg.getNoteNumber(), msg.getVelocity(), event.sampleOffset);
            else if (msg.isNoteOff())
                noteOff(msg.getNoteNumber(), event.sampleOffset);
            else if (msg.isController() && (msg.getControllerNumber() == 123 || msg.getControllerNumber() == 120))
                allNotesOff();
        }

        auto* left = audio.getChannel(0);
        auto* right = audio.getNumChannels() > 1 ? audio.getChannel(1) : nullptr;

        for (int s = 0; s < numSamples; ++s)
        {
            float out = 0.0f;

            for (auto& v : voices)
            {
                if (!v.active)
                    continue;

                out += std::sin(static_cast<float>(v.phase)) * v.level;
                v.phase += v.phaseIncrement;

                v.level *= 0.99995f;
                if (v.level < 0.0001f)
                    v.active = false;
            }

            out = std::tanh(out * 0.5f);

            left[s] = out;
            if (right != nullptr)
                right[s] = out;
        }
    }

private:
    // ... Voice struct, voices array, noteOn/noteOff/allNotesOff unchanged ...
    // ... Remove the SimpleSynthProcessor(const&) = delete at the end ...
};

} // namespace dc
```

**Key changes from current:**
- Remove `#include <JuceHeader.h>` and `#include "MidiBridge.h"`
- Remove `juce::AudioProcessor` base, all JUCE boilerplate overrides
- `processBlock` → `process(AudioBlock&, MidiBlock&, int)`
- Replace `buffer.clear()` → `audio.clear()`
- Replace `buffer.getWritePointer(ch)` → `audio.getChannel(ch)`
- Replace `buffer.getNumChannels()` → `audio.getNumChannels()`
- Replace `buffer.getNumSamples()` → `numSamples`
- Replace `auto dcMidi = bridge::fromJuce(midiMessages);` → iterate `midi` directly
- Remove `BusesProperties()` constructor

## Scope Limitation

- Only modify the 3 processor files listed above (5 source files: 2 `.h` + 2 `.cpp` + 1 header-only)
- Do NOT delete `MidiBridge.h` — it may still be referenced by `AudioEngine.cpp` until
  Agent 04 integrates everything. The bridge is deleted in Agent 04.
- Do NOT modify `AudioEngine.h/.cpp`, `BounceProcessor`, or `CMakeLists.txt`
- Do NOT modify audio-only processors (`TrackProcessor`, `MixBusProcessor`, etc.)

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
- The build WILL have errors in `AudioEngine.cpp` and `BounceProcessor.cpp` (they still
  use `juce::AudioProcessorGraph`). That's expected — Agent 04 handles integration.
