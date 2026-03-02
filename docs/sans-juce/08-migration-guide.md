# 08 — Migration Guide: File-by-File Checklist

> Every source file grouped by migration phase, with specific JUCE APIs to
> replace and compilation verification steps.

**Related**: All spec documents ([00-prd.md](00-prd.md) through [07-undo-system.md](07-undo-system.md))

---

## How to Use This Guide

1. Work through phases in order (0 → 1 → 2 → 3 → 4 → 5)
2. Within each phase, process files in the order listed
3. After each file, run the verification command to confirm compilation
4. Check off files as completed

**Verification command** (run after each file or batch):
```bash
cmake --build --preset release 2>&1 | head -50
```

**Final verification** (after each phase):
```bash
cmake --build --preset release && ./build/DremCanvas_artefacts/Release/DremCanvas
```

---

## Phase 0 — Foundation Sweep

**Spec**: [04-foundation-types.md](04-foundation-types.md)

**Goal**: Replace all JUCE foundation types with `std::` / `dc::` equivalents.
This phase touches nearly every file but makes only mechanical changes.

**Pre-requisite**: Create `dc/foundation/` header library with:
- `types.h`, `string_utils.h`, `file_utils.h`, `assert.h`, `time.h`
- `base64.h`, `message_queue.h`, `listener_list.h`, `spsc_queue.h`
- `worker_thread.h`, `config.h.in`

### Application Entry Point

| File | JUCE APIs to replace |
|------|---------------------|
| `src/Main.cpp` | `JUCEApplication`, `START_JUCE_APPLICATION`, `String`, `File`, `Timer`, `Time::currentTimeMillis`, `KeyPress` |

**Changes**: Replace JUCE application class with custom `main()` + `dc::Application`.
Remove `JuceHeader.h` include.

### Model Layer (foundation types only — ValueTree stays until Phase 1)

| File | JUCE APIs to replace |
|------|---------------------|
| `src/model/Project.h` | `String`, `File`, `Colour`, `Random` |
| `src/model/Project.cpp` | `String`, `File`, `Colour`, `Random` |
| `src/model/Track.h` | `String`, `Colour`, `Random` |
| `src/model/Track.cpp` | `String`, `Colour`, `Random` |
| `src/model/Arrangement.h` | `String` |
| `src/model/Arrangement.cpp` | `String` |
| `src/model/AudioClip.h` | `String`, `File` |
| `src/model/AudioClip.cpp` | `String`, `File` |
| `src/model/MidiClip.h` | `String`, `MemoryOutputStream`, `MemoryInputStream` |
| `src/model/MidiClip.cpp` | `MemoryBlock`, `Base64` |
| `src/model/StepSequencer.h` | `String`, `Random` |
| `src/model/StepSequencer.cpp` | `Random` |
| `src/model/MixerState.h` | (minimal) |
| `src/model/MixerState.cpp` | (minimal) |
| `src/model/TempoMap.h` | (minimal) |
| `src/model/TempoMap.cpp` | (minimal) |
| `src/model/Clipboard.h` | (minimal) |
| `src/model/Clipboard.cpp` | (minimal) |
| `src/model/GridSystem.h` | (minimal) |
| `src/model/GridSystem.cpp` | (minimal) |
| `src/model/RecentProjects.h` | `File`, `String` |
| `src/model/RecentProjects.cpp` | `File`, `String` |
| `src/model/serialization/YAMLSerializer.h` | `String`, `File`, `Colour` |
| `src/model/serialization/YAMLSerializer.cpp` | `String`, `File`, `Colour`, `Base64` |
| `src/model/serialization/SessionWriter.h` | `String`, `File` |
| `src/model/serialization/SessionWriter.cpp` | `String`, `File` |
| `src/model/serialization/SessionReader.h` | `String`, `File` |
| `src/model/serialization/SessionReader.cpp` | `String`, `File`, `parseXML` (remove legacy XML path) |

### Engine Layer (foundation types only — AudioProcessor stays until Phase 3)

| File | JUCE APIs to replace |
|------|---------------------|
| `src/engine/AudioEngine.h` | `String`, `File` |
| `src/engine/AudioEngine.cpp` | `String`, `File` |
| `src/engine/TransportController.h` | `Time::currentTimeMillis` |
| `src/engine/TransportController.cpp` | `Time` |
| `src/engine/TrackProcessor.h` | `String`, `File`, `CriticalSection` → `std::mutex` |
| `src/engine/TrackProcessor.cpp` | `String`, `File` |
| `src/engine/MidiEngine.h` | `CriticalSection` → `std::mutex`, `String` |
| `src/engine/MidiEngine.cpp` | `CriticalSection`, `ScopedLock`, `MessageManager::callAsync` → `dc::MessageQueue`, `Time` |
| `src/engine/AudioRecorder.h` | `String`, `File`, `TimeSliceThread` → `dc::WorkerThread` |
| `src/engine/AudioRecorder.cpp` | `String`, `File` |
| `src/engine/BounceProcessor.h` | `String`, `File` |
| `src/engine/BounceProcessor.cpp` | `String`, `File` |
| `src/engine/MixBusProcessor.h` | (minimal) |
| `src/engine/MixBusProcessor.cpp` | (minimal) |
| `src/engine/MeterTapProcessor.h` | (minimal) |
| `src/engine/MeterTapProcessor.cpp` | (minimal) |
| `src/engine/MetronomeProcessor.h` | `MathConstants::twoPi` → `dc::pi * 2`, `Random` |
| `src/engine/MetronomeProcessor.cpp` | math constants |
| `src/engine/MidiClipProcessor.h` | (minimal) |
| `src/engine/MidiClipProcessor.cpp` | (minimal) |
| `src/engine/StepSequencerProcessor.h` | `Random` |
| `src/engine/StepSequencerProcessor.cpp` | `Random` |
| `src/engine/SimpleSynthProcessor.h` | `MathConstants`, `Random` |

### Plugin Layer (foundation types only — plugin hosting stays until Phase 4)

| File | JUCE APIs to replace |
|------|---------------------|
| `src/plugins/PluginManager.h` | `String`, `File`, `StringArray` |
| `src/plugins/PluginManager.cpp` | `String`, `File` |
| `src/plugins/PluginHost.h` | `String`, `MemoryBlock`, `Base64` |
| `src/plugins/PluginHost.cpp` | `String`, `MemoryBlock`, `Base64` |
| `src/plugins/PluginWindowManager.h` | `String` |
| `src/plugins/PluginWindowManager.cpp` | `String` |
| `src/plugins/PluginEditorBridge.h` | `String` |
| `src/plugins/PluginEditorBridge.cpp` | `String` |
| `src/plugins/ParameterFinderScanner.h` | `String`, `Time` |
| `src/plugins/ParameterFinderScanner.cpp` | `String`, `Time`, `MessageManager::callAsync` |
| `src/plugins/SyntheticInputProbe.h` | (minimal) |
| `src/plugins/SyntheticInputProbe.cpp` | (minimal) |
| `src/plugins/VST3ParameterFinderSupport.h` | (interface — minimal JUCE) |

### Vim Engine

| File | JUCE APIs to replace |
|------|---------------------|
| `src/vim/VimEngine.h` | `KeyListener` → remove (already bridged to gfx::KeyEvent), `Time`, `String` |
| `src/vim/VimEngine.cpp` | `KeyPress`, `Time::currentTimeMillis` → `dc::currentTimeMillis`, `String` |
| `src/vim/VimContext.h` | `String` |
| `src/vim/VimContext.cpp` | `String` |
| `src/vim/ActionRegistry.h` | `String` |
| `src/vim/ActionRegistry.cpp` | `String` |
| `src/vim/VirtualKeyboardState.h` | (minimal) |

### Utils

| File | JUCE APIs to replace |
|------|---------------------|
| `src/utils/UndoSystem.h` | `Time::currentTimeMillis`, `String` (UndoManager stays until Phase 1) |
| `src/utils/UndoSystem.cpp` | `Time`, `String` |
| `src/utils/AudioFileUtils.h` | `String`, `File` (AudioFormatManager stays until Phase 2) |
| `src/utils/AudioFileUtils.cpp` | `String`, `File` |
| `src/utils/GitIntegration.h` | `String`, `File` |
| `src/utils/GitIntegration.cpp` | `String`, `File`, `ChildProcess` → `popen()` or `std::system()` |

### Graphics Engine

| File | JUCE APIs to replace |
|------|---------------------|
| `src/graphics/core/Widget.h` | `ValueTree::Listener` (stays until Phase 1, but `File` and `String` go now) |
| `src/graphics/core/Widget.cpp` | (minimal) |
| `src/graphics/rendering/WaveformCache.h` | `File`, `String`, `ChangeListener` → `dc::ListenerList` callback |
| `src/graphics/rendering/WaveformCache.cpp` | `File`, `String` |
| `src/graphics/theme/FontManager.h` | `File` |
| `src/graphics/theme/FontManager.cpp` | `File` |

### GUI (Legacy JUCE Components)

These files are heavily JUCE-dependent (Component, Timer, Graphics, etc.).
In Phase 0, only replace foundation types (`String`, `File`, `Colour`).
The JUCE Component/GUI code is removed when `gui/` is eventually deleted
(replaced by `ui/` widgets that use the custom graphics engine).

| File | Phase 0 changes |
|------|----------------|
| `src/gui/MainComponent.h/.cpp` | `String`, `File`, `Colour` |
| `src/gui/MainWindow.h/.cpp` | `String` |
| `src/gui/common/DremLookAndFeel.h/.cpp` | `Colour` → `dc::Colour` (conversion needed for juce::Graphics) |
| `src/gui/arrangement/ArrangementView.h/.cpp` | `String`, `Colour` |
| `src/gui/arrangement/TrackLane.h/.cpp` | `String`, `File`, `Colour` |
| `src/gui/arrangement/TimeRuler.h/.cpp` | `String`, `Colour` |
| `src/gui/arrangement/WaveformView.h/.cpp` | `File`, `String` |
| `src/gui/arrangement/MidiClipView.h/.cpp` | `Colour` |
| `src/gui/arrangement/AutomationLane.h/.cpp` | `Colour` |
| `src/gui/arrangement/Cursor.h/.cpp` | `Colour` |
| `src/gui/mixer/MixerPanel.h/.cpp` | `String`, `Colour` |
| `src/gui/mixer/ChannelStrip.h/.cpp` | `String`, `Colour` |
| `src/gui/mixer/MeterComponent.h/.cpp` | `Colour` |
| `src/gui/mixer/PluginSlotList.h/.cpp` | `String` |
| `src/gui/midieditor/PianoRollEditor.h/.cpp` | `String`, `Colour` |
| `src/gui/midieditor/PianoKeyboard.h/.cpp` | `Colour` |
| `src/gui/midieditor/NoteComponent.h/.cpp` | `Colour` |
| `src/gui/sequencer/StepSequencerView.h/.cpp` | `String`, `Colour` |
| `src/gui/sequencer/StepGrid.h/.cpp` | `Colour` |
| `src/gui/sequencer/StepButton.h/.cpp` | `Colour` |
| `src/gui/sequencer/PatternSelector.h/.cpp` | `String` |
| `src/gui/transport/TransportBar.h/.cpp` | `String` |
| `src/gui/vim/VimStatusBar.h/.cpp` | `String` |
| `src/gui/browser/BrowserPanel.h/.cpp` | `String` |

### UI (New Widget Layer)

| File | Phase 0 changes |
|------|----------------|
| `src/ui/AppController.h/.cpp` | `String`, `File`, `Time` |
| All `src/ui/**/*Widget.h/.cpp` | `String`, `Colour` (most use these via ValueTree access) |

### Platform Layer

| File | Phase 0 changes |
|------|----------------|
| `src/platform/EventBridge.h/.mm` | Minimal JUCE (already native) |
| `src/platform/NativeWindow.h/.mm` | `String` (title), `File` (icon) |
| `src/platform/MetalView.h/.mm` | (no JUCE) |
| `src/platform/NativeDialogs.h/.mm` | `String`, `File` |
| `src/platform/MacPluginEditorBridge.h/.mm` | `Component` (stays until Phase 4) |
| `src/platform/MacSyntheticInputProbe.h/.mm` | (minimal) |
| `src/platform/linux/GlfwWindow.h/.cpp` | (no JUCE) |
| `src/platform/linux/VulkanBackend.h/.cpp` | (no JUCE) |
| `src/platform/linux/NativeDialogs.cpp` | `String`, `File` |
| `src/platform/linux/EmbeddedPluginEditor.h/.cpp` | `Component` (stays until Phase 4) |
| `src/platform/linux/X11PluginEditorBridge.h/.cpp` | `Component` (stays until Phase 4) |
| `src/platform/linux/X11Compositor.h/.cpp` | (no JUCE) |
| `src/platform/linux/X11MouseProbe.h/.cpp` | (no JUCE) |
| `src/platform/linux/X11Reparent.h/.cpp` | (no JUCE) |
| `src/platform/linux/X11SyntheticInputProbe.h/.cpp` | (no JUCE) |

### Phase 0 Verification

```bash
# After all foundation type replacements:
grep -rn "juce::String\|juce::File\|juce::Array\|juce::Colour\|juce::Random" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm" | \
    grep -v "juce::AudioProcessor\|juce::MidiBuffer\|juce::ValueTree\|juce::UndoManager\|juce::AudioPlugin\|juce::KnownPlugin\|juce::Component\|juce::Timer\|juce::KeyListener\|juce::Slider\|juce::Label\|juce::Graphics\|juce::ChangeListener\|juce::Viewport\|juce::ListBox\|juce::LookAndFeel\|juce::MouseEvent\|juce::KeyPress\|juce::Font\|juce::Rectangle\|juce::AudioBuffer\|juce::MidiMessage\|juce::AudioDeviceManager\|juce::AudioFormatManager\|juce::AudioFormatReader\|juce::MidiInput\|juce::FileOutputStream\|juce::FileInputSource\|juce::FileChooser\|juce::FileBrowserComponent\|juce::ColourGradient\|juce::AlertWindow\|juce::DocumentWindow\|juce::OutputStream\|ColourBridge\|JUCE API boundary\|getProgramName\|changeProgramName\|getName.*override"
# Should return zero hits (all remaining juce:: are Phase 2-5 boundary types)
```

---

## Phase 1 — Observable Model + Undo

**Specs**: [01-observable-model.md](01-observable-model.md), [07-undo-system.md](07-undo-system.md)

**Goal**: Replace `ValueTree`, `Identifier`, `var`, `UndoManager`.

### New files to create

- `src/dc/model/PropertyTree.h/.cpp`
- `src/dc/model/PropertyId.h/.cpp`
- `src/dc/model/Variant.h/.cpp`
- `src/dc/model/UndoManager.h/.cpp`
- `src/dc/model/UndoAction.h/.cpp`

### Files to migrate

| File | Changes |
|------|---------|
| `src/model/Project.h/.cpp` | `ValueTree` → `PropertyTree`, `Identifier` → `PropertyId`, `var` → `Variant`, `UndoManager` → `dc::UndoManager` |
| `src/model/Track.h/.cpp` | Same pattern |
| `src/model/Arrangement.h/.cpp` | Same pattern |
| `src/model/AudioClip.h/.cpp` | Same pattern |
| `src/model/MidiClip.h/.cpp` | Same pattern + `MidiMessageSequence` → `dc::MidiSequence` (Phase 2 dependency, can stub) |
| `src/model/StepSequencer.h/.cpp` | Same pattern |
| `src/model/MixerState.h/.cpp` | Same pattern |
| `src/model/TempoMap.h/.cpp` | Same pattern |
| `src/model/Clipboard.h/.cpp` | `ValueTree::createDeepCopy()` → `PropertyTree::createDeepCopy()` |
| `src/model/GridSystem.h/.cpp` | Same pattern |
| `src/model/RecentProjects.h/.cpp` | Same pattern |
| `src/model/serialization/YAMLSerializer.h/.cpp` | All ValueTree access → PropertyTree |
| `src/model/serialization/SessionWriter.h/.cpp` | Same |
| `src/model/serialization/SessionReader.h/.cpp` | Same |
| `src/utils/UndoSystem.h/.cpp` | `juce::UndoManager` → `dc::UndoManager` |
| `src/graphics/core/Widget.h/.cpp` | `ValueTree::Listener` → `PropertyTree::Listener` |
| All `src/ui/**/*Widget.h/.cpp` | `valueTreePropertyChanged` → `propertyChanged`, etc. |
| All `src/gui/**/*.h/.cpp` | Same listener callback renames |

### Phase 1 Verification

```bash
grep -rn "juce::ValueTree\|juce::Identifier\|juce::var\b\|juce::UndoManager" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm"
# Should return zero hits

# Functional test:
# 1. Launch app, create session, save
# 2. Reload session, verify all data intact
# 3. Make changes, undo (u), redo (Ctrl+R), verify
```

---

## Phase 2 — Audio I/O + MIDI

**Specs**: [05-audio-io.md](05-audio-io.md), [06-midi-subsystem.md](06-midi-subsystem.md)

### New files to create

- `src/dc/audio/AudioDeviceManager.h/.cpp`
- `src/dc/audio/PortAudioDeviceManager.h/.cpp`
- `src/dc/audio/AudioFileReader.h/.cpp`
- `src/dc/audio/AudioFileWriter.h/.cpp`
- `src/dc/audio/DiskStreamer.h/.cpp`
- `src/dc/audio/ThreadedRecorder.h/.cpp`
- `src/dc/midi/MidiMessage.h/.cpp`
- `src/dc/midi/MidiBuffer.h/.cpp`
- `src/dc/midi/MidiSequence.h/.cpp`
- `src/dc/midi/MidiDeviceManager.h/.cpp`

### Files to migrate

| File | Changes |
|------|---------|
| `src/engine/AudioEngine.h/.cpp` | `juce::AudioDeviceManager` → `dc::AudioDeviceManager`, `juce::AudioProcessorPlayer` → custom callback |
| `src/engine/MidiEngine.h/.cpp` | `juce::MidiInput` → `dc::MidiDeviceManager`, `juce::MidiInputCallback` → `dc::MidiInputCallback`, `CriticalSection` → `dc::SPSCQueue` |
| `src/engine/AudioRecorder.h/.cpp` | `juce::AudioFormatWriter::ThreadedWriter` → `dc::ThreadedRecorder` |
| `src/engine/TrackProcessor.h/.cpp` | `juce::AudioBuffer<float>` → `dc::AudioBlock`, `juce::MidiBuffer` → `dc::MidiBlock` |
| `src/engine/MidiClipProcessor.h/.cpp` | `juce::MidiBuffer` → `dc::MidiBlock`, `juce::MidiMessage` → `dc::MidiMessage` |
| `src/engine/StepSequencerProcessor.h/.cpp` | Same MIDI type changes |
| `src/engine/SimpleSynthProcessor.h` | Same MIDI type changes |
| `src/engine/MetronomeProcessor.h/.cpp` | `juce::MidiBuffer` → `dc::MidiBlock` (pass-through) |
| `src/engine/MixBusProcessor.h/.cpp` | `juce::AudioBuffer` → `dc::AudioBlock`, `juce::MidiBuffer` → `dc::MidiBlock` |
| `src/engine/MeterTapProcessor.h/.cpp` | Same buffer type changes |
| `src/engine/BounceProcessor.h/.cpp` | `juce::AudioFormatWriter` → `dc::AudioFileWriter` |
| `src/model/MidiClip.h/.cpp` | `juce::MidiMessageSequence` → `dc::MidiSequence`, binary serialization format |
| `src/utils/AudioFileUtils.h/.cpp` | `juce::AudioFormatManager` → `dc::AudioFileReader` |
| `src/graphics/rendering/WaveformCache.h/.cpp` | `juce::AudioFormatReader` → `dc::AudioFileReader` |

### Phase 2 Verification

```bash
grep -rn "juce::MidiMessage\|juce::MidiBuffer\|juce::MidiMessageSequence\|juce::MidiInput\|juce::AudioDeviceManager\|juce::AudioFormatManager\|juce::AudioFormatReader\|juce::AudioFormatWriter\|juce::AudioBuffer" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm"
# Should return zero hits

# Functional test:
# 1. Launch app, verify audio output works
# 2. Load session with audio clips, verify playback
# 3. Record audio, verify file saved correctly
# 4. Connect MIDI controller, verify MIDI input
# 5. Verify waveform display
```

---

## Phase 3 — Audio Graph

**Spec**: [02-audio-graph.md](02-audio-graph.md)

### New files to create

- `src/dc/engine/AudioNode.h`
- `src/dc/engine/AudioBlock.h/.cpp`
- `src/dc/engine/MidiBlock.h/.cpp`
- `src/dc/engine/AudioGraph.h/.cpp`
- `src/dc/engine/BufferPool.h/.cpp`
- `src/dc/engine/GraphExecutor.h/.cpp`
- `src/dc/engine/DelayNode.h/.cpp`

### Files to migrate

| File | Changes |
|------|---------|
| `src/engine/AudioEngine.h/.cpp` | `juce::AudioProcessorGraph` → `dc::AudioGraph`, `NodeID` → `dc::NodeId`, remove `AudioProcessorPlayer` |
| `src/engine/TrackProcessor.h/.cpp` | `juce::AudioProcessor` → `dc::AudioNode`, `processBlock` → `process`, remove boilerplate |
| `src/engine/MixBusProcessor.h/.cpp` | Same AudioProcessor → AudioNode migration |
| `src/engine/MeterTapProcessor.h/.cpp` | Same |
| `src/engine/MetronomeProcessor.h/.cpp` | Same |
| `src/engine/MidiClipProcessor.h/.cpp` | Same |
| `src/engine/StepSequencerProcessor.h/.cpp` | Same |
| `src/engine/SimpleSynthProcessor.h` | Same |
| `src/engine/AudioRecorder.h/.cpp` | Same |
| `src/engine/BounceProcessor.h/.cpp` | Same |
| `src/engine/MidiEngine.h/.cpp` | Same |

### Phase 3 Verification

```bash
grep -rn "juce::AudioProcessor\|juce::AudioProcessorGraph\|AudioProcessorPlayer" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm" | \
    grep -v "juce::AudioPluginInstance\|juce::AudioProcessorEditor"
# Should return zero hits (plugin-related AudioProcessor stays until Phase 4)

# Functional test:
# 1. Multi-track playback (verify all tracks mixed correctly)
# 2. Add/remove tracks during playback (topology changes)
# 3. Verify metering works
# 4. Verify metronome works
# 5. Verify recording works through graph
```

---

## Phase 4 — VST3 Plugin Hosting

**Spec**: [03-plugin-hosting.md](03-plugin-hosting.md)

### New files to create

- `src/dc/plugins/VST3Module.h/.cpp`
- `src/dc/plugins/VST3Host.h/.cpp`
- `src/dc/plugins/PluginInstance.h/.cpp`
- `src/dc/plugins/PluginDescription.h`
- `src/dc/plugins/PluginScanner.h/.cpp`
- `src/dc/plugins/PluginEditor.h/.cpp`
- `src/dc/plugins/ComponentHandler.h/.cpp`

### Files to migrate

| File | Changes |
|------|---------|
| `src/plugins/PluginManager.h/.cpp` | `AudioPluginFormatManager` → `dc::VST3Host`, `KnownPluginList` → `dc::VST3Host` database, `PluginDirectoryScanner` → `dc::PluginScanner`, `PluginDescription` → `dc::PluginDescription` |
| `src/plugins/PluginHost.h/.cpp` | `AudioPluginInstance` → `dc::PluginInstance`, `createPluginInstanceAsync` → `VST3Host::createInstance`, state save/restore with `vector<uint8_t>` |
| `src/plugins/PluginWindowManager.h/.cpp` | `AudioProcessorEditor` → `dc::PluginEditor` |
| `src/plugins/PluginEditorBridge.h/.cpp` | Update bridge interface for dc::PluginEditor |
| `src/plugins/ParameterFinderScanner.h/.cpp` | Remove JUCE patch workarounds, use native `IParameterFinder` and `IComponentHandler` |
| `src/plugins/SyntheticInputProbe.h/.cpp` | Remove any JUCE types |
| `src/plugins/VST3ParameterFinderSupport.h` | Simplify or remove (native SDK access) |
| `src/platform/MacPluginEditorBridge.h/.mm` | `AudioProcessorEditor`/`Component` → `dc::PluginEditor` |
| `src/platform/linux/X11PluginEditorBridge.h/.cpp` | Same |
| `src/platform/linux/EmbeddedPluginEditor.h/.cpp` | Same |
| `src/engine/TrackProcessor.h/.cpp` | Plugin chain: `AudioPluginInstance` → `dc::PluginInstance` |

### Phase 4 Verification

```bash
grep -rn "juce::AudioPluginInstance\|juce::AudioProcessorEditor\|juce::AudioPluginFormatManager\|juce::KnownPluginList\|juce::PluginDescription\|juce::PluginDirectoryScanner" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm"
# Should return zero hits

# Functional test:
# 1. Scan for plugins (verify all known VST3s found)
# 2. Load a VST3 plugin, verify audio processing
# 3. Open plugin editor, verify UI displays
# 4. Save/restore plugin state, verify round-trip
# 5. Parameter finder overlay, verify spatial hints work
# 6. Test with yabridge-bridged plugin
# 7. Load existing session with plugins, verify compatibility
```

---

## Phase 5 — Final Cleanup

**Goal**: Remove all remaining JUCE traces.

### Legacy GUI removal

The `src/gui/` directory contains JUCE-based components that are superseded
by `src/ui/` widgets. These files can be deleted entirely:

| Directory | Files to delete |
|-----------|----------------|
| `src/gui/MainComponent.h/.cpp` | Legacy JUCE Component |
| `src/gui/MainWindow.h/.cpp` | Legacy JUCE DocumentWindow |
| `src/gui/common/DremLookAndFeel.h/.cpp` | JUCE LookAndFeel |
| `src/gui/arrangement/*.h/*.cpp` | 7 files (ArrangementView, TrackLane, TimeRuler, WaveformView, MidiClipView, AutomationLane, Cursor) |
| `src/gui/mixer/*.h/*.cpp` | 5 files (MixerPanel, ChannelStrip, MeterComponent, PluginSlotList) |
| `src/gui/midieditor/*.h/*.cpp` | 3 files (PianoRollEditor, PianoKeyboard, NoteComponent) |
| `src/gui/sequencer/*.h/*.cpp` | 4 files (StepSequencerView, StepGrid, StepButton, PatternSelector) |
| `src/gui/transport/TransportBar.h/.cpp` | Legacy transport |
| `src/gui/vim/VimStatusBar.h/.cpp` | Legacy status bar |
| `src/gui/browser/BrowserPanel.h/.cpp` | Legacy browser |

**Total**: ~25 file pairs to delete

### Build system cleanup

| File | Changes |
|------|---------|
| `CMakeLists.txt` | Remove `juce_add_gui_app`, remove all `juce::juce_*` link targets, remove JUCE compile definitions, remove `gui/` sources from `target_sources` |
| `libs/JUCE/` | Delete submodule |
| `.gitmodules` | Remove JUCE entry |
| `scripts/juce-patches/` | Delete directory |
| `scripts/bootstrap.sh` | Remove JUCE clone/patch code |
| `scripts/check_deps.sh` | Remove JUCE checks, add PortAudio/libsndfile/RtMidi/VST3SDK checks |

### VimEngine cleanup

| File | Changes |
|------|---------|
| `src/vim/VimEngine.h` | Remove `juce::KeyListener` base class (already using custom event dispatch) |
| `src/vim/VimEngine.cpp` | Remove `juce::KeyPress` usage |

### Final verification

```bash
# Zero JUCE symbols
grep -rn "juce::" src/ --include="*.h" --include="*.cpp" --include="*.mm"
# Should return zero hits

# Zero JUCE includes
grep -rn "JuceHeader\|juce_" src/ --include="*.h" --include="*.cpp" --include="*.mm"
# Should return zero hits

# No JUCE in CMake
grep -n "juce" CMakeLists.txt
# Should return zero hits

# Clean build
rm -rf build/
cmake --preset release
cmake --build --preset release

# No JUCE symbols in binary
nm build/DremCanvas_artefacts/Release/DremCanvas | grep -i juce
# Should return zero hits

# Launch and test
./build/DremCanvas_artefacts/Release/DremCanvas
```

---

## Summary: File Counts by Phase

| Phase | Files modified | Files created | Files deleted |
|-------|---------------|---------------|---------------|
| 0 — Foundation | ~100 | ~12 (dc/foundation/) | 0 |
| 1 — Model + Undo | ~50 | ~5 (PropertyTree, Variant, etc.) | 0 |
| 2 — Audio I/O + MIDI | ~20 | ~10 (audio/midi wrappers) | 0 |
| 3 — Audio Graph | ~15 | ~7 (AudioNode, AudioGraph, etc.) | 0 |
| 4 — Plugin Hosting | ~15 | ~7 (VST3Host, PluginInstance, etc.) | 0 |
| 5 — Cleanup | ~5 | 0 | ~50 (gui/, JUCE submodule, patches) |
| **Total** | ~205 | ~41 | ~50 |
