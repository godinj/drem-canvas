# 02 — Sans-JUCE Migration Tests

> Test-by-test specification for every dc:: type that replaced a JUCE equivalent,
> covering foundation (Phase 0), model (Phase 1), and audio/MIDI (Phase 2).

**Phase**: 3–6 (Foundation, Model, MIDI, Audio tests)
**Dependencies**: Phase 1 (CMake infrastructure)
**Related**: [00-prd.md](00-prd.md), [01-cmake-infrastructure.md](01-cmake-infrastructure.md),
[../sans-juce/04-foundation-types.md](../sans-juce/04-foundation-types.md),
[../sans-juce/01-observable-model.md](../sans-juce/01-observable-model.md),
[../sans-juce/06-midi-subsystem.md](../sans-juce/06-midi-subsystem.md),
[../sans-juce/05-audio-io.md](../sans-juce/05-audio-io.md)

---

## Overview

The sans-JUCE migration created four dc:: libraries totalling ~2,800 LOC. Each class
replaced a well-tested JUCE equivalent. These tests prove that the replacements
implement the same contracts the application code depends on.

Tests are organised by library and listed in implementation order. Each test file
specifies the class under test, the invariants to verify, and the edge cases to cover.

---

## Phase 0 — Foundation Tests

### test_colour.cpp

**Class**: `dc::Colour` (replaces `juce::Colour`)
**File**: `src/dc/foundation/types.h`

| Test Case | Invariant |
|-----------|-----------|
| `fromRGB(r,g,b)` produces correct ARGB uint32 | Alpha defaults to 0xFF |
| `fromFloat(r,g,b,a)` clamps to [0,1] | Values outside range are clamped, not wrapped |
| `fromHSV(h,s,v,a)` round-trips through HSV/RGB | `toHSV(fromHSV(h,s,v)) ≈ (h,s,v)` within ε |
| `fromHSV()` with S=0 (grayscale) | Hue is irrelevant; R=G=B=V×255 |
| `toHexString()` / `fromHexString()` round-trip | `fromHexString(c.toHexString()) == c` |
| `fromHexString()` with invalid chars | Returns black or throws (document behaviour) |
| `brighter()` at white stays white | No overflow beyond 0xFF |
| `darker()` at black stays black | No underflow below 0x00 |
| `interpolatedWith()` at t=0 | Returns `this` colour |
| `interpolatedWith()` at t=1 | Returns `other` colour |
| `interpolatedWith()` at t=0.5 | Midpoint of each channel |
| `withAlpha()` preserves RGB channels | Only alpha byte changes |
| All `Colours::` presets | Spot-check: `Colours::red == fromRGB(255,0,0)` |

### test_string_utils.cpp

**Functions**: `dc::trim()`, `dc::replace()`, `dc::contains()`, `dc::startsWith()`,
`dc::afterFirst()`, `dc::shellQuote()`, `dc::format()`
**File**: `src/dc/foundation/string_utils.h`

| Test Case | Invariant |
|-----------|-----------|
| `trim("")` | Returns empty string |
| `trim("  hello  ")` | Returns `"hello"` |
| `trim()` on all-whitespace | Returns empty string |
| `replace()` all occurrences | `replace("aaba", "a", "x")` → `"xxbx"` |
| `replace()` with empty `from` | No-op (returns original) |
| `replace()` overlapping | Greedy left-to-right: `replace("aaa", "aa", "b")` → `"ba"` |
| `contains()` substring | True for present, false for absent |
| `startsWith()` prefix | True for match, false for mismatch, true for empty prefix |
| `afterFirst()` delimiter at end | Returns empty string |
| `afterFirst()` delimiter absent | Returns empty string |
| `shellQuote()` with spaces | Wraps in single quotes |
| `shellQuote()` with embedded `'` | Escapes via `'\''` |
| `shellQuote()` empty string | Returns `''` |
| `shellQuote()` with control chars | Characters preserved inside quotes |
| `format()` valid format string | `format("x=%d", 42)` → `"x=42"` |
| `format()` no args | Returns format string as-is |

### test_base64.cpp

**Functions**: `dc::base64Encode()`, `dc::base64Decode()`
**File**: `src/dc/foundation/base64.h`, `src/dc/foundation/base64.cpp`

| Test Case | Invariant |
|-----------|-----------|
| Round-trip for sizes 0, 1, 2, 3, 4, 5, 100, 1000 | `decode(encode(data)) == data` |
| Empty input encode | Returns empty string |
| Empty input decode | Returns empty vector |
| Whitespace tolerance in decode | `decode("aG Vs\nbG8=")` succeeds |
| Invalid characters silently skipped | Non-base64 chars in input |
| Padding variations | 0, 1, 2 padding `=` chars |

### test_random.cpp

**Functions**: `dc::randomFloat()`, `dc::randomInt()`
**File**: `src/dc/foundation/types.h`

| Test Case | Invariant |
|-----------|-----------|
| `randomFloat()` range | 1000 calls all in [0.0, 1.0) |
| `randomInt(min, max)` range | 1000 calls all in [min, max] inclusive |
| `randomInt(5, 5)` | Always returns 5 |
| Thread safety | 10 threads calling concurrently, no crashes (thread-local RNG) |

### test_spsc_queue.cpp

**Class**: `dc::SpscQueue<T>` (lock-free single-producer single-consumer)
**File**: `src/dc/foundation/spsc_queue.h`

| Test Case | Invariant |
|-----------|-----------|
| Push/pop correctness | Push 1,2,3; pop returns 1,2,3 in order |
| Push to full queue | Returns `false`, element not added |
| Pop from empty queue | Returns `false`, output unchanged |
| Capacity rounds to power of 2 | `SpscQueue<int>(5)` has capacity ≥ 8 |
| Wrap-around | Push capacity items, pop all, push again — works correctly |
| Multi-threaded stress test | Producer pushes 100k sequential ints, consumer verifies FIFO order |
| Move-only types | `SpscQueue<std::unique_ptr<int>>` compiles and works |

### test_message_queue.cpp

**Class**: `dc::MessageQueue`
**File**: `src/dc/foundation/message_queue.h`, `src/dc/foundation/message_queue.cpp`

| Test Case | Invariant |
|-----------|-----------|
| `post()` then `processAll()` | Callback fires |
| FIFO ordering | Post A, B, C; processAll fires A, B, C in order |
| `pending()` count | Reflects queued callbacks |
| Callback posts new callback | Deferred to next `processAll()` call |
| Multi-thread post | 10 threads posting, processAll on main thread, no crashes |

### test_listener_list.cpp

**Class**: `dc::ListenerList<T>`
**File**: `src/dc/foundation/listener_list.h`

| Test Case | Invariant |
|-----------|-----------|
| Add and call | Listener receives callback |
| Remove stops callbacks | After remove, callback not received |
| Duplicate add silently ignored | Adding same listener twice, called once |
| Remove during callback | Safe (iterates a copy) |
| Add during callback | New listener not called in current iteration |
| Call on empty list | No-op, no crash |
| Remove listener not in list | No-op, no crash |

### test_worker_thread.cpp

**Class**: `dc::WorkerThread`
**File**: `src/dc/foundation/worker_thread.h`, `src/dc/foundation/worker_thread.cpp`

| Test Case | Invariant |
|-----------|-----------|
| Submit and execute | Task runs on background thread |
| Task ordering | Tasks execute in submission order |
| `stop()` waits for current task | Currently executing task completes |
| `stop()` multiple times | Idempotent, no crash |
| Submit after `stop()` | Silently ignored |
| `isRunning()` state | True after construction, false after stop |

---

## Phase 1 — Model Tests

### test_variant.cpp

**Class**: `dc::Variant` (replaces `juce::var`)
**File**: `src/dc/model/Variant.h`, `src/dc/model/Variant.cpp`

| Test Case | Invariant |
|-----------|-----------|
| All six type tags construct correctly | Void, Int, Double, Bool, String, Binary |
| `int` constructor promotes to `int64_t` | `Variant(42).toInt() == 42` |
| Strict `toInt()` on String throws `TypeMismatch` | Wrong type → exception |
| Strict `toString()` on Int throws `TypeMismatch` | Wrong type → exception |
| `getIntOr()` on Double truncates | `Variant(3.7).getIntOr(0) == 3` |
| `getDoubleOr()` on Int promotes | `Variant(5).getDoubleOr(0.0) == 5.0` |
| `getBoolOr()` on Int | 0 → false, 1 → true, -1 → true |
| Equality: same type + same value | `Variant(42) == Variant(42)` |
| Equality: different types | `Variant(42) != Variant(42.0)` |
| `Void == Void` | Always true |
| Empty string vs Void | Not equal |
| Binary blob round-trip | Store and retrieve `vector<uint8_t>` |
| Copy and move semantics | Copy produces equal variant; move leaves source as Void |

### test_property_id.cpp

**Class**: `dc::PropertyId` (string interning)
**File**: `src/dc/model/PropertyId.h`, `src/dc/model/PropertyId.cpp`

| Test Case | Invariant |
|-----------|-----------|
| Same string → same pointer | `PropertyId("x") == PropertyId("x")` via O(1) pointer compare |
| Different strings → different | `PropertyId("x") != PropertyId("y")` |
| Hash consistency | `Hash{}(id1) == Hash{}(id1)` across calls |
| Use in `std::unordered_map` | Insert and lookup work correctly |
| Construct from `const char*` | Works identically to `string_view` |
| Empty string | Valid PropertyId, distinct from all non-empty |
| Thread-safe interning | 10 threads interning same string simultaneously |

### test_property_tree.cpp

**Class**: `dc::PropertyTree` (replaces `juce::ValueTree`)
**File**: `src/dc/model/PropertyTree.h`, `src/dc/model/PropertyTree.cpp`

This is the most critical test file — PropertyTree is the single source of truth for
all model state.

**Property Operations**:

| Test Case | Invariant |
|-----------|-----------|
| `setProperty()` / `getProperty()` round-trip | All Variant types |
| `getProperty()` on missing key | Returns Void |
| `hasProperty()` | True after set, false after remove |
| `removeProperty()` | Property gone, `hasProperty()` returns false |
| `getNumProperties()` | Increments on set, decrements on remove |
| `getPropertyName(index)` | Returns correct PropertyId |
| Set same value again | No listener notification if value unchanged |

**Child Operations**:

| Test Case | Invariant |
|-----------|-----------|
| `addChild()` / `getChild()` | Child retrievable by index |
| `addChild()` at index 0, middle, end, -1 (append) | Correct insertion position |
| `removeChild(index)` | Child count decrements, subsequent children shift |
| `removeChild(child)` | Finds and removes by identity |
| `removeAllChildren()` | Empty after call |
| `moveChild(oldIndex, newIndex)` | Child at new position, others shift |
| `getChild()` out of bounds | Returns invalid tree |
| `getChildWithType()` | Finds first child with matching type |
| `getChildWithProperty()` | Finds first child with matching property value |
| `indexOf()` | Returns correct index, -1 for non-child |
| Move child to same position | No-op, no listener notification |

**Parent Tracking**:

| Test Case | Invariant |
|-----------|-----------|
| `getParent()` after `addChild()` | Returns parent tree |
| Adding child to new parent | Removes from old parent automatically |
| Root tree `getParent()` | Returns invalid tree |

**Listener Contract**:

| Test Case | Invariant |
|-----------|-----------|
| `propertyChanged` fires on `setProperty()` | Receives tree, propertyId |
| `childAdded` fires on `addChild()` | Receives parent, child |
| `childRemoved` fires on `removeChild()` | Receives parent, child, index |
| `childOrderChanged` fires on `moveChild()` | Receives parent, old/new index |
| `parentChanged` fires when re-parented | Receives child tree |
| Remove listener during callback | No crash, callback completes |
| Listener on parent notified of child property change | Bubbling behaviour |

**Deep Copy**:

| Test Case | Invariant |
|-----------|-----------|
| `createDeepCopy()` produces equal tree | All properties and children match |
| Mutations on copy do not affect original | Independent instances |
| Copy has no parent | `getParent()` returns invalid |

**Undo Integration**:

| Test Case | Invariant |
|-----------|-----------|
| `setProperty(id, val, &undoManager)` records action | Action in undo stack |
| `undo()` restores previous value | Property reverts |
| `redo()` re-applies | Property returns to new value |
| `addChild()` with undo | Child removal on undo |
| `removeChild()` with undo | Child re-added on undo |

**Iterator**:

| Test Case | Invariant |
|-----------|-----------|
| Range-for over children | Visits all children in order |
| Empty tree: `begin() == end()` | No iterations |
| Iterator invalidation on add/remove during iteration | Document and test behaviour |

### test_undo_manager.cpp

**Class**: `dc::UndoManager` (replaces `juce::UndoManager`)
**File**: `src/dc/model/UndoManager.h`, `src/dc/model/UndoManager.cpp`

| Test Case | Invariant |
|-----------|-----------|
| Single action: undo restores, redo re-applies | Symmetric |
| Transaction grouping | `beginTransaction()` + N actions → single undo step |
| Redo stack cleared on new action | After undo then new action, `canRedo()` is false |
| Action coalescing via `tryMerge()` | Same-property edits merge into one |
| `setMaxTransactions()` | Oldest transactions discarded when limit reached |
| `canUndo()` / `canRedo()` | False when stacks are empty |
| `undo()` / `redo()` on empty stack | Returns false |
| `clearHistory()` | Both stacks emptied |
| Multiple `beginTransaction()` calls | Idempotent (only first takes effect) |
| `getUndoDescription()` / `getRedoDescription()` | Returns action name |

---

## Phase 2 — MIDI Tests

### test_midi_message.cpp

**Class**: `dc::MidiMessage` (replaces `juce::MidiMessage`)
**File**: `src/dc/midi/MidiMessage.h`, `src/dc/midi/MidiMessage.cpp`

| Test Case | Invariant |
|-----------|-----------|
| `noteOn(ch, note, vel)` | Correct status byte (0x9n), channel 1-indexed |
| `noteOff(ch, note, vel)` | Status byte 0x8n |
| `controllerEvent(ch, cc, val)` | Status byte 0xBn |
| `programChange(ch, program)` | Status byte 0xCn |
| `pitchWheel(ch, value)` | Status byte 0xEn, 14-bit value |
| `aftertouch(ch, note, pressure)` | Status byte 0xAn |
| `channelPressure(ch, pressure)` | Status byte 0xDn |
| `allNotesOff(ch)` | CC 123, value 0 |
| `allSoundOff(ch)` | CC 120, value 0 |
| Channel clamping: 0 → 1 | Minimum channel is 1 |
| Channel clamping: 17 → 16 | Maximum channel is 16 |
| Velocity clamping | Negative → 0, > 1.0 → 1.0 |
| Note clamping | Negative → 0, ≥ 128 → 127 |
| `isNoteOff()` for velocity-0 noteOn | 0x90 with vel=0 treated as noteOff |
| `getVelocity()` returns float [0,1] | Scaled from raw [0,127] |
| `getRawVelocity()` returns int [0,127] | Raw byte |
| Raw data round-trip | `MidiMessage(msg.getRawData(), msg.getRawDataSize())` reconstructs original |
| SysEx message | `isSysEx()` true, data larger than 3 bytes stored in heap buffer |
| Empty message | `getRawDataSize() == 0` |
| `setChannel()`, `setNoteNumber()`, `setVelocity()` mutations | Values updated correctly |

### test_midi_buffer.cpp

**Class**: `dc::MidiBuffer` (replaces `juce::MidiBuffer`)
**File**: `src/dc/midi/MidiBuffer.h`, `src/dc/midi/MidiBuffer.cpp`

| Test Case | Invariant |
|-----------|-----------|
| `addEvent()` then iterate | Event retrieved with correct sampleOffset and message |
| Multiple events at same offset | All preserved in insertion order |
| `clear()` empties buffer | `isEmpty()` is true, `getNumEvents()` is 0 |
| Iterator reconstructs MidiMessage | From flat byte layout |
| Negative sampleOffset | Accepted (pre-roll scenarios) |
| Large sampleOffset | No overflow |
| SysEx event in buffer | Variable-length message stored correctly |
| Empty buffer iteration | `begin() == end()` |

### test_midi_sequence.cpp

**Class**: `dc::MidiSequence` (replaces JUCE `MidiMessageSequence`)
**File**: `src/dc/midi/MidiSequence.h`, `src/dc/midi/MidiSequence.cpp`

| Test Case | Invariant |
|-----------|-----------|
| `addEvent()` maintains sorted order | Events sorted by `timeInBeats` |
| `updateMatchedPairs()` links noteOn/Off | `matchedPairIndex` points to partner |
| Unmatched noteOn | `matchedPairIndex` stays -1 |
| Multiple noteOff for same note | First unmatched noteOff matches |
| `getEventsInRange(start, end)` | Returns correct [start, end) index range |
| Range with no events | Returns empty range |
| `removeEvent(index)` | Event removed, subsequent indices shift |
| `sort()` after manual manipulation | Re-sorts correctly |
| Binary serialization round-trip | `fromBinary(seq.toBinary()) == seq` |
| Legacy JUCE format deserialization | Big-endian double + int32 header parsed correctly |
| `getEvents()` returns const reference | No copy |

---

## Phase 2 — Audio Tests

### test_audio_block.cpp

**Class**: `dc::AudioBlock` (non-owning buffer view)
**File**: `src/dc/audio/AudioBlock.h`

| Test Case | Invariant |
|-----------|-----------|
| `getChannel(ch)` returns correct pointer | Points to caller-owned memory |
| `getNumChannels()` / `getNumSamples()` | Match constructor args |
| `clear()` zeros all samples | Every sample in every channel is 0.0f |
| Zero channels | Valid construction, no crash |
| Zero samples | Valid construction, no crash |
| Const correctness | `const AudioBlock` only exposes `const float*` |

### test_audio_file_io.cpp

**Classes**: `dc::AudioFileReader`, `dc::AudioFileWriter`
**Files**: `src/dc/audio/AudioFileReader.h/.cpp`, `src/dc/audio/AudioFileWriter.h/.cpp`

These are integration tests requiring temporary files.

| Test Case | Invariant |
|-----------|-----------|
| WAV 16-bit write/read round-trip | Samples match within quantization error |
| WAV 24-bit write/read round-trip | Higher precision |
| WAV 32-float write/read round-trip | Lossless for float samples |
| AIFF 16-bit round-trip | Cross-format support |
| AIFF 24-bit round-trip | Cross-format support |
| FLAC 16-bit round-trip | Compressed format |
| FLAC 24-bit round-trip | Compressed format |
| AudioBlock interleave/de-interleave | Write AudioBlock, read back into AudioBlock, compare |
| `open()` non-existent file | Returns nullptr |
| `open()` non-audio file | Returns nullptr |
| Read past end of file | Returns fewer frames than requested |
| Multi-channel (mono, stereo, 5.1) | Channel counts preserved |
| `getFormatName()`, `getBitDepth()` | Correct metadata |

### test_disk_streamer.cpp

**Class**: `dc::DiskStreamer` (background read thread with ring buffers)
**File**: `src/dc/audio/DiskStreamer.h`, `src/dc/audio/DiskStreamer.cpp`

| Test Case | Invariant |
|-----------|-----------|
| `open()` + `start()` + `read()` | Returns audio data matching file content |
| `read()` before `start()` | Returns silence (zeros) |
| `seek()` to middle of file | Subsequent reads start from seek position |
| `seek()` past EOF | Clamps to end, reads return 0 frames |
| Sequential `open()` calls | Previous file closed, new file opened |
| Small buffer size (1 frame) | Still works correctly |
| `getLengthInSamples()`, `getSampleRate()`, `getNumChannels()` | Match source file |

### test_threaded_recorder.cpp

**Class**: `dc::ThreadedRecorder` (background write thread)
**File**: `src/dc/audio/ThreadedRecorder.h`, `src/dc/audio/ThreadedRecorder.cpp`

| Test Case | Invariant |
|-----------|-----------|
| Record audio, stop, verify file | File contains submitted samples |
| `write()` never blocks audio thread | Returns immediately (lock-free path) |
| Overflow (write faster than disk) | Samples dropped, no crash, no block |
| `stop()` without `start()` | No crash |
| `stop()` flushes remaining data | All buffered samples written to disk |

---

## Property-Based Tests (RapidCheck)

These complement the table-driven tests above with randomised input:

```cpp
rc::prop("Variant round-trip: any int stored and retrieved matches",
    [](int64_t val) {
        dc::Variant v(val);
        RC_ASSERT(v.toInt() == val);
    });

rc::prop("PropertyTree: deep copy is independent",
    [](/* generated tree */) {
        auto copy = tree.createDeepCopy();
        // Mutate original, verify copy unchanged
    });

rc::prop("MidiSequence: addEvent maintains sorted order",
    [](const std::vector<std::pair<double, int>>& events) {
        dc::MidiSequence seq;
        for (auto& [time, note] : events)
            seq.addEvent(dc::MidiMessage::noteOn(1, note % 128, 0.8f), time);
        for (int i = 1; i < seq.getNumEvents(); ++i)
            RC_ASSERT(seq.getEvent(i).timeInBeats >= seq.getEvent(i-1).timeInBeats);
    });

rc::prop("base64 round-trip for any binary data",
    [](const std::vector<uint8_t>& data) {
        RC_ASSERT(dc::base64Decode(dc::base64Encode(data)) == data);
    });

rc::prop("SPSCQueue: all pushed items are popped in FIFO order",
    [](const std::vector<int>& items) {
        RC_PRE(items.size() <= 1024);
        dc::SpscQueue<int> q(1024);
        for (auto& item : items) q.push(item);
        for (auto& item : items) {
            int popped;
            RC_ASSERT(q.pop(popped));
            RC_ASSERT(popped == item);
        }
    });
```

---

## Test Fixtures

Audio tests require fixture files in `tests/fixtures/`:

| File | Format | Channels | Sample Rate | Duration | Content |
|------|--------|----------|-------------|----------|---------|
| `test_mono_44100.wav` | WAV 16-bit | 1 | 44100 | 1s | 440 Hz sine |
| `test_stereo_48000.wav` | WAV 16-bit | 2 | 48000 | 1s | 440 Hz L, 880 Hz R |
| `test_mono_44100.flac` | FLAC 16-bit | 1 | 44100 | 1s | 440 Hz sine |

Generate with a test helper or `sox`:

```bash
sox -n -r 44100 -c 1 -b 16 tests/fixtures/test_mono_44100.wav synth 1 sine 440
sox -n -r 48000 -c 2 -b 16 tests/fixtures/test_stereo_48000.wav synth 1 sine 440 sine 880
sox -n -r 44100 -c 1 -b 16 tests/fixtures/test_mono_44100.flac synth 1 sine 440
```
