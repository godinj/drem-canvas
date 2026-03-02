# 06 — MIDI Subsystem

> Replaces `juce::MidiMessage`, `juce::MidiBuffer`, `juce::MidiMessageSequence`,
> `juce::MidiInput`, and `juce::AbstractFifo` with custom types and RtMidi.

**Phase**: 2 (Audio I/O + MIDI)
**Dependencies**: Phase 0 (Foundation Types)
**Related**: [02-audio-graph.md](02-audio-graph.md), [05-audio-io.md](05-audio-io.md), [08-migration-guide.md](08-migration-guide.md)

---

## Overview

Drem Canvas uses JUCE MIDI types in three layers:

1. **Audio thread** — `juce::MidiBuffer` passes timestamped events through the
   processor graph (per-block)
2. **Model layer** — `juce::MidiMessageSequence` stores MIDI clips as sorted
   event lists, serialized to base64 binary
3. **Device I/O** — `juce::MidiInput` / `juce::MidiInputCallback` for real-time
   MIDI recording from hardware

The replacements:
- `dc::MidiMessage` — compact 3-byte message struct
- `dc::MidiBuffer` — flat buffer for audio thread (replaces `juce::MidiBuffer`)
- `dc::MidiSequence` — sorted vector for model layer (replaces `juce::MidiMessageSequence`)
- `dc::MidiDeviceManager` — RtMidi wrapper for hardware I/O
- `dc::SPSCQueue<T>` — lock-free ring buffer (replaces `juce::AbstractFifo`)

---

## Current JUCE MIDI Usage

### MidiMessage factory methods

```cpp
juce::MidiMessage::noteOn(channel, noteNumber, velocity);
juce::MidiMessage::noteOff(channel, noteNumber, velocity);
juce::MidiMessage::controllerEvent(channel, ccNumber, value);
juce::MidiMessage::programChange(channel, programNumber);
juce::MidiMessage::pitchWheel(channel, position);
juce::MidiMessage::allNotesOff(channel);
```

### MidiBuffer (audio thread)

```cpp
juce::MidiBuffer midiBuffer;
for (auto metadata : midiBuffer)
{
    auto msg = metadata.getMessage();
    int samplePosition = metadata.samplePosition;
    // process msg...
}
midiBuffer.addEvent(msg, samplePosition);
midiBuffer.clear();
```

### MidiMessageSequence (model layer, MidiClip.h)

```cpp
juce::MidiMessageSequence sequence;
sequence.addEvent(juce::MidiMessage::noteOn(...), timeInTicks);
sequence.addEvent(juce::MidiMessage::noteOff(...), timeInTicks);
sequence.sort();
sequence.updateMatchedPairs();

// Serialization to base64
juce::MemoryOutputStream mos;
// write sequence...
auto base64 = mos.getMemoryBlock().toBase64Encoding();
```

### MidiInput / MidiInputCallback (MidiEngine.h)

```cpp
class MidiEngine : public juce::MidiInputCallback
{
    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& msg) override;
};

auto devices = juce::MidiInput::getAvailableDevices();
auto input = juce::MidiInput::openDevice(deviceId, this);
input->start();
```

### SPSC Communication (MidiEngine → audio thread)

```cpp
// Currently uses juce::CriticalSection (not ideal)
juce::CriticalSection midiLock;
std::vector<juce::MidiMessage> pendingMessages;

// In handleIncomingMidiMessage (MIDI thread):
{
    const juce::ScopedLock sl(midiLock);
    pendingMessages.push_back(msg);
}

// In processBlock (audio thread):
{
    const juce::ScopedLock sl(midiLock);
    for (auto& msg : pendingMessages)
        midiBuffer.addEvent(msg, 0);
    pendingMessages.clear();
}
```

This lock-based approach is replaced with a lock-free SPSC queue.

---

## Design: `dc::MidiMessage`

Compact message struct. Stores up to 3 bytes for channel messages,
variable-length for SysEx.

```cpp
namespace dc {

class MidiMessage
{
public:
    // --- Constructors ---
    MidiMessage() = default;
    MidiMessage(uint8_t status, uint8_t data1, uint8_t data2 = 0);
    MidiMessage(const uint8_t* data, int size);  // SysEx

    // --- Factory methods ---
    static MidiMessage noteOn(int channel, int noteNumber, float velocity);
    static MidiMessage noteOff(int channel, int noteNumber, float velocity = 0.0f);
    static MidiMessage controllerEvent(int channel, int controller, int value);
    static MidiMessage programChange(int channel, int program);
    static MidiMessage pitchWheel(int channel, int value);  // 0-16383, center=8192
    static MidiMessage channelPressure(int channel, int pressure);
    static MidiMessage aftertouch(int channel, int noteNumber, int pressure);
    static MidiMessage allNotesOff(int channel);
    static MidiMessage allSoundOff(int channel);

    // --- Queries ---
    bool isNoteOn() const;
    bool isNoteOff() const;
    bool isNoteOnOrOff() const;
    bool isController() const;
    bool isProgramChange() const;
    bool isPitchWheel() const;
    bool isChannelPressure() const;
    bool isAftertouch() const;
    bool isSysEx() const;

    int getChannel() const;        // 1-16
    int getNoteNumber() const;     // 0-127
    float getVelocity() const;     // 0.0-1.0
    int getRawVelocity() const;    // 0-127
    int getControllerNumber() const;
    int getControllerValue() const;
    int getProgramChangeNumber() const;
    int getPitchWheelValue() const;

    // --- Raw access ---
    const uint8_t* getRawData() const;
    int getRawDataSize() const;

    // --- Mutation ---
    void setChannel(int channel);
    void setNoteNumber(int noteNumber);
    void setVelocity(float velocity);

private:
    // Short messages: inline 3 bytes
    // SysEx: heap-allocated (rare in real-time path)
    uint8_t data_[3] = {0, 0, 0};
    int size_ = 0;
    std::vector<uint8_t> sysex_;  // only for SysEx messages
};

} // namespace dc
```

**Design notes**:
- Channel is 1-based in the public API (matching MIDI spec and JUCE convention)
- Velocity is `float` 0-1 (matching JUCE `MidiMessage::getFloatVelocity()`)
- SysEx messages use heap allocation (acceptable — they're not on the hot path)
- 3-byte inline storage keeps the struct small for cache-friendly buffer iteration

---

## Design: `dc::MidiBuffer`

Flat buffer for passing MIDI events through the audio graph. Each event is
stored as `{sampleOffset, size, data[]}` in a contiguous byte array.

```cpp
namespace dc {

class MidiBuffer
{
public:
    MidiBuffer();
    explicit MidiBuffer(int initialCapacity);

    /// Add an event at the given sample offset
    void addEvent(const MidiMessage& msg, int sampleOffset);

    /// Clear all events
    void clear();

    /// Number of events
    int getNumEvents() const;

    /// Whether the buffer is empty
    bool isEmpty() const;

    // --- Iteration ---

    struct Event
    {
        int sampleOffset;
        MidiMessage message;
    };

    class Iterator
    {
    public:
        Event operator*() const;
        Iterator& operator++();
        bool operator!=(const Iterator& other) const;
    };

    Iterator begin() const;
    Iterator end() const;

private:
    // Internal storage: flat byte array
    // Layout per event: [int32 sampleOffset][int16 size][uint8 data...]
    std::vector<uint8_t> data_;
    int numEvents_ = 0;
};

} // namespace dc
```

**Storage format**: Events are packed sequentially in `data_`. Each event:
```
[4 bytes: sample offset (int32)]
[2 bytes: message size (int16)]
[N bytes: raw MIDI data]
```

This matches the JUCE MidiBuffer internal layout for cache-friendly iteration.

---

## Design: `dc::MidiSequence`

Sorted vector of timestamped MIDI events for the model layer. Replaces
`juce::MidiMessageSequence`.

```cpp
namespace dc {

struct TimedMidiEvent
{
    double timeInBeats;  // position in beats (not samples)
    MidiMessage message;
    int matchedPairIndex = -1;  // index of matching noteOff (for noteOn events)
};

class MidiSequence
{
public:
    MidiSequence() = default;

    /// Add an event (maintains sorted order by timeInBeats)
    void addEvent(const MidiMessage& msg, double timeInBeats);

    /// Remove an event by index
    void removeEvent(int index);

    /// Get event by index
    const TimedMidiEvent& getEvent(int index) const;
    TimedMidiEvent& getEvent(int index);

    /// Number of events
    int getNumEvents() const;

    /// Clear all events
    void clear();

    /// Sort events by timestamp (called after bulk modifications)
    void sort();

    /// Match noteOn/noteOff pairs (sets matchedPairIndex)
    void updateMatchedPairs();

    /// Get events in a time range (for playback)
    /// Returns indices of events where startBeats <= time < endBeats
    std::pair<int, int> getEventsInRange(double startBeats,
                                          double endBeats) const;

    // --- Serialization ---

    /// Serialize to binary (for base64 encoding in YAML)
    std::vector<uint8_t> toBinary() const;

    /// Deserialize from binary
    static MidiSequence fromBinary(const std::vector<uint8_t>& data);

    // --- Direct access ---
    const std::vector<TimedMidiEvent>& getEvents() const;

private:
    std::vector<TimedMidiEvent> events_;
};

} // namespace dc
```

### Binary Serialization Format

The existing JUCE serialization uses `MemoryOutputStream` / `MemoryInputStream`
with JUCE's internal MidiMessage encoding. The new format:

```
[4 bytes: version (uint32, = 1)]
[4 bytes: numEvents (uint32)]
For each event:
  [8 bytes: timeInBeats (double)]
  [2 bytes: messageSize (uint16)]
  [N bytes: raw MIDI data]
```

This is simpler and doesn't depend on any JUCE serialization code.

**Migration**: Existing sessions store MIDI as JUCE-format base64. A one-time
migration reads the old format and re-encodes in the new format. The reader
detects the format by checking the version header.

---

## Design: `dc::MidiDeviceManager`

RtMidi wrapper for hardware MIDI I/O. Replaces `juce::MidiInput` and related
device enumeration APIs.

```cpp
namespace dc {

/// MIDI device information
struct MidiDeviceInfo
{
    int index;
    std::string name;
    bool isInput;
};

/// Callback for incoming MIDI messages
class MidiInputCallback
{
public:
    virtual ~MidiInputCallback() = default;
    virtual void handleMidiMessage(const MidiMessage& msg,
                                    double timestamp) = 0;
};

class MidiDeviceManager
{
public:
    MidiDeviceManager();
    ~MidiDeviceManager();

    /// Enumerate available input devices
    std::vector<MidiDeviceInfo> getInputDevices() const;

    /// Enumerate available output devices
    std::vector<MidiDeviceInfo> getOutputDevices() const;

    /// Open an input device and start receiving messages
    bool openInput(int deviceIndex, MidiInputCallback* callback);

    /// Close an input device
    void closeInput(int deviceIndex);

    /// Open an output device
    bool openOutput(int deviceIndex);

    /// Send a message to an output device
    void sendMessage(int deviceIndex, const MidiMessage& msg);

    /// Close an output device
    void closeOutput(int deviceIndex);

    /// Close all devices
    void closeAll();

private:
    struct InputPort
    {
        std::unique_ptr<RtMidiIn> rtMidi;
        MidiInputCallback* callback = nullptr;
    };

    struct OutputPort
    {
        std::unique_ptr<RtMidiOut> rtMidi;
    };

    std::unordered_map<int, InputPort> inputs_;
    std::unordered_map<int, OutputPort> outputs_;

    /// RtMidi callback (static, bridges to MidiInputCallback)
    static void rtMidiCallback(double timeStamp,
                                std::vector<unsigned char>* message,
                                void* userData);
};

} // namespace dc
```

### RtMidi Integration

RtMidi calls back on its own thread. The callback translates the raw bytes
into `dc::MidiMessage` and forwards to the registered `MidiInputCallback`.

The existing `MidiEngine` replaces `juce::MidiInputCallback` with
`dc::MidiInputCallback` and swaps `juce::MidiInput` for `dc::MidiDeviceManager`.

---

## Design: `dc::SPSCQueue<T>`

Lock-free single-producer single-consumer ring buffer. Replaces the
`juce::CriticalSection`-based approach in `MidiEngine` and also replaces
`juce::AbstractFifo` used elsewhere.

```cpp
namespace dc {

template<typename T>
class SPSCQueue
{
public:
    explicit SPSCQueue(size_t capacity);

    /// Push an item (producer thread). Returns false if full.
    bool push(const T& item);
    bool push(T&& item);

    /// Push multiple items. Returns number actually pushed.
    size_t push(const T* items, size_t count);

    /// Pop an item (consumer thread). Returns false if empty.
    bool pop(T& item);

    /// Pop multiple items. Returns number actually popped.
    size_t pop(T* items, size_t maxCount);

    /// Number of items available to read
    size_t available() const;

    /// Whether the queue is empty
    bool isEmpty() const;

    /// Whether the queue is full
    bool isFull() const;

private:
    std::vector<T> buffer_;
    size_t capacity_;
    std::atomic<size_t> readPos_{0};
    std::atomic<size_t> writePos_{0};
};

} // namespace dc
```

**Memory ordering**: Uses `std::memory_order_acquire` for reads of the
opposite position and `std::memory_order_release` for writes of the local
position. This provides the necessary happens-before relationship without
full barriers.

### MIDI Recording Flow (Replacing Lock-Based Approach)

```
RtMidi callback thread:              Audio thread:
  MidiMessage msg = ...              for each block:
  spscQueue.push(msg);                 MidiMessage msg;
                                       while (spscQueue.pop(msg))
                                         midiBuffer.addEvent(msg, 0);
```

No locks, no contention. The SPSC queue is sized for worst-case burst
(e.g., 256 messages).

---

## Migration Path

### Step 1: Implement dc::MidiMessage

Direct replacement for `juce::MidiMessage`. Factory methods have identical
signatures. Test all message types.

### Step 2: Implement dc::MidiBuffer

Replace `juce::MidiBuffer` in processor `process()` signatures. The iteration
pattern changes slightly:

```cpp
// Before (JUCE)
for (auto metadata : midiBuffer)
{
    auto msg = metadata.getMessage();
    int pos = metadata.samplePosition;
}

// After (dc)
for (auto event : midiBuffer)
{
    auto& msg = event.message;
    int pos = event.sampleOffset;
}
```

### Step 3: Implement dc::MidiSequence

Replace `juce::MidiMessageSequence` in `MidiClip`. Update serialization to
use new binary format with backward-compatible reader.

### Step 4: Implement dc::SPSCQueue

Replace `CriticalSection` + `std::vector` in `MidiEngine` with lock-free queue.

### Step 5: Implement dc::MidiDeviceManager (RtMidi)

Replace `juce::MidiInput` device enumeration and callback. Update `MidiEngine`
to use `dc::MidiDeviceManager`.

---

## Files Affected

| File | JUCE APIs replaced |
|------|-------------------|
| `src/engine/MidiEngine.h/.cpp` | `MidiInput`, `MidiInputCallback`, `CriticalSection` |
| `src/engine/TrackProcessor.h/.cpp` | `MidiBuffer` (parameter type) |
| `src/engine/MidiClipProcessor.h` | `MidiBuffer`, `MidiMessage` |
| `src/engine/StepSequencerProcessor.h` | `MidiBuffer`, `MidiMessage` |
| `src/engine/SimpleSynthProcessor.h` | `MidiBuffer`, `MidiMessage` |
| `src/engine/MetronomeProcessor.h` | `MidiBuffer` (unused but in signature) |
| `src/engine/MixBusProcessor.h` | `MidiBuffer` (pass-through) |
| `src/engine/MeterTapProcessor.h` | `MidiBuffer` (pass-through) |
| `src/model/MidiClip.h/.cpp` | `MidiMessageSequence`, `MemoryOutputStream` |
| All processors | `processBlock(AudioBuffer, MidiBuffer)` → `process(AudioBlock, MidiBlock, int)` |

## CMake Integration

### RtMidi

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTMIDI REQUIRED rtmidi)
target_link_libraries(DremCanvas PRIVATE ${RTMIDI_LIBRARIES})
target_include_directories(DremCanvas PRIVATE ${RTMIDI_INCLUDE_DIRS})
```

System packages:
```bash
# Debian/Ubuntu
sudo apt install librtmidi-dev

# Fedora
sudo dnf install rtmidi-devel

# macOS
brew install rtmidi
```

## Testing Strategy

1. **MidiMessage**: Create all message types, verify byte encoding
2. **MidiBuffer**: Add events at various offsets, iterate, verify order
3. **MidiSequence**: Add events, sort, verify order. Serialize/deserialize round-trip
4. **SPSCQueue**: Producer/consumer threads, verify all messages delivered in order
5. **MidiDeviceManager**: Enumerate devices (may need MIDI loopback for CI)
6. **Integration**: Record MIDI from hardware, verify timestamps and note accuracy
