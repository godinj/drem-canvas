# Agent: Audio Processor Migration

You are working on the `feature/sans-juce-audio-graph` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 3 of the Sans-JUCE migration: migrate the 4 audio-only processor classes
from `juce::AudioProcessor` to `dc::AudioNode`.

## Context

Read these specs before starting:
- `docs/sans-juce/02-audio-graph.md` (AudioNode interface, migration table)
- `docs/sans-juce/08-migration-guide.md` (Phase 3 section)
- `src/engine/TrackProcessor.h` and `.cpp` (current implementation)
- `src/engine/MixBusProcessor.h` and `.cpp` (current implementation)
- `src/engine/MeterTapProcessor.h` and `.cpp` (current implementation)
- `src/engine/MetronomeProcessor.h` and `.cpp` (current implementation)
- `src/dc/audio/AudioBlock.h` (existing AudioBlock — your process() methods receive this)

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

### Migration pattern (applied to all 4 processors)

Each processor follows this transformation:

**Remove:**
- `#include <JuceHeader.h>`
- `: public juce::AudioProcessor` base class
- `BusesProperties()` constructor delegation
- All JUCE boilerplate overrides:
  - `getName()` returning `juce::String`
  - `getTailLengthSeconds()`
  - `createEditor()`, `hasEditor()`
  - `getNumPrograms()`, `getCurrentProgram()`, `setCurrentProgram()`
  - `getProgramName()`, `changeProgramName()`
  - `getStateInformation()`, `setStateInformation()`

**Replace:**
- `: public juce::AudioProcessor` → `: public dc::AudioNode`
- `#include <JuceHeader.h>` → `#include "dc/engine/AudioNode.h"` and `#include "dc/engine/MidiBlock.h"`
- `void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)` → `void prepare(double sampleRate, int maxBlockSize) override`
- `void releaseResources()` → `void release() override`
- `void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)` → `void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override`
- `bool acceptsMidi() const override` / `bool producesMidi() const override` — keep, they're part of AudioNode too

**Add:**
- `std::string getName() const override { return "<Name>"; }`
- `int getNumInputChannels() const override { return N; }` (where appropriate)
- `int getNumOutputChannels() const override { return 2; }`

**In process() body:**
- Remove `dc::AudioBlock block(buffer.getArrayOfWritePointers(), ...)` wrapper construction.
  The `audio` parameter IS the AudioBlock now — use it directly.
- Replace `buffer.getNumChannels()` → `audio.getNumChannels()`
- Replace `buffer.getNumSamples()` → `numSamples`
- Replace `buffer.clear()` → `audio.clear()`
- Replace `buffer.applyGain(ch, start, num, gain)` → `audio.applyGain(ch, start, num, gain)`
- Replace `buffer.getMagnitude(ch, start, num)` → inline loop:
  ```cpp
  float mag = 0.0f;
  const float* data = audio.getChannel(ch);
  for (int i = start; i < start + num; ++i)
      mag = std::max(mag, std::abs(data[i]));
  ```

### 1. `src/engine/TrackProcessor.h`

Current: inherits `juce::AudioProcessor`, stereo output, no MIDI.

After migration:
```cpp
#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "TransportController.h"
#include "dc/audio/DiskStreamer.h"
#include <filesystem>
#include <memory>
#include <atomic>

namespace dc {

class TrackProcessor : public AudioNode
{
public:
    TrackProcessor(TransportController& transport);
    ~TrackProcessor() override;

    bool loadFile(const std::filesystem::path& file);
    void clearFile();

    // AudioNode interface
    void prepare(double sampleRate, int maxBlockSize) override;
    void release() override;
    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override;

    std::string getName() const override { return "TrackProcessor"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }

    // Gain/pan (unchanged)
    void setGain(float g) { gain.store(g); }
    float getGain() const { return gain.load(); }
    void setPan(float p) { pan.store(p); }
    float getPan() const { return pan.load(); }
    void setMuted(bool m) { muted.store(m); }
    bool isMuted() const { return muted.load(); }

    int64_t getFileLengthInSamples() const;

    // Metering
    float getPeakLevelLeft() const { return peakLeft.load(); }
    float getPeakLevelRight() const { return peakRight.load(); }

private:
    // ... same private members, no changes ...
};

} // namespace dc
```

### 2. `src/engine/TrackProcessor.cpp`

Key changes in `process()`:
- Signature: `void TrackProcessor::process(AudioBlock& audio, MidiBlock& /*midi*/, int numSamples)`
- Remove the `dc::AudioBlock block(...)` wrapper — use `audio` directly
- Replace `buffer.getNumChannels()` → `audio.getNumChannels()`
- Replace `buffer.getNumSamples()` → `numSamples`
- Replace `buffer.clear()` → `audio.clear()`
- Replace `buffer.applyGain(ch, start, num, gain)` → `audio.applyGain(ch, 0, numSamples, gain)`
- Replace `buffer.getMagnitude(ch, start, num)` with inline peak calculation
- Replace `lastSeekPosition = posInSamples + buffer.getNumSamples()` → `lastSeekPosition = posInSamples + numSamples`

Constructor: Remove `AudioProcessor(BusesProperties()...)` base init. Just `TrackProcessor(TransportController& transport) : transportController(transport) {}`

### 3. `src/engine/MixBusProcessor.h`

After migration: inherits `dc::AudioNode`, stereo in + stereo out.

- `int getNumInputChannels() const override { return 2; }`
- `int getNumOutputChannels() const override { return 2; }`
- `std::string getName() const override { return "MixBus"; }`

### 4. `src/engine/MixBusProcessor.cpp`

Constructor: Remove `AudioProcessor(BusesProperties()...)`. Just `MixBusProcessor(TransportController& transport) : transportController(transport) {}`

`process()`: Already uses `dc::AudioBlock block(...)` internally. Replace with direct `audio` parameter usage. The logic (apply gain, compute peaks) stays the same.

### 5. `src/engine/MeterTapProcessor.h`

After migration: inherits `dc::AudioNode`, stereo in + stereo out, pass-through with metering.

- `int getNumInputChannels() const override { return 2; }`
- `int getNumOutputChannels() const override { return 2; }`
- `std::string getName() const override { return "MeterTap"; }`

### 6. `src/engine/MeterTapProcessor.cpp`

Constructor: Remove `AudioProcessor(BusesProperties()...)`. Default constructor: `MeterTapProcessor() = default;` or `MeterTapProcessor() {}`.

`process()`: Already uses `dc::AudioBlock block(...)` internally. Replace with direct `audio` parameter. Peak measurement logic unchanged.

### 7. `src/engine/MetronomeProcessor.h`

After migration: inherits `dc::AudioNode`, no input, stereo output.

- `int getNumInputChannels() const override { return 0; }`
- `int getNumOutputChannels() const override { return 2; }`
- `std::string getName() const override { return "Metronome"; }`

### 8. `src/engine/MetronomeProcessor.cpp`

Constructor: Remove `AudioProcessor(BusesProperties()...)`. Just `MetronomeProcessor(TransportController& transport) : transportController(transport) {}`.

`process()`: Already uses `dc::AudioBlock block(...)` internally. Replace with direct `audio` parameter. Click synthesis logic unchanged.

## Scope Limitation

- Only modify the 4 files listed above (8 source files total: 4 `.h` + 4 `.cpp`)
- Do NOT modify `AudioEngine.h/.cpp`, `BounceProcessor`, `MidiBridge.h`, or `CMakeLists.txt`
- Do NOT modify MIDI-aware processors (`MidiClipProcessor`, `StepSequencerProcessor`, `SimpleSynthProcessor`)
- Do NOT remove `#include <JuceHeader.h>` from files you don't own

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
- The build WILL have errors in `AudioEngine.cpp` (it still creates processors as
  `juce::AudioProcessor`). That's expected — Agent 04 handles the integration.
  Verify your files compile individually by checking for zero errors in the 4 processor
  translation units.
