# Agent: GUI Deletion + Build System Cleanup

You are working on the `feature/sans-juce-cleanup` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 5 (Final Cleanup): delete the legacy JUCE GUI layer, remove JUCE
from the build system entirely, delete the JUCE submodule and patches, and update scripts.

## Context

Read these specs before starting:
- `docs/sans-juce/00-prd.md` (Phase 5 — Final Cleanup deliverables, Success Criteria)
- `docs/sans-juce/08-migration-guide.md` (Phase 5 — Legacy GUI removal, Build system cleanup, Final verification)
- `CMakeLists.txt` (line 12: `add_subdirectory(libs/JUCE)`, lines 117-125: `juce_add_gui_app`, line 125: `juce_generate_juce_header`, lines 395-416: JUCE compile defs, lines 420-431: `juce::juce_*` link targets)
- `src/gui/` directory listing (49 files across arrangement/, browser/, common/, midieditor/, mixer/, sequencer/, transport/, vim/)
- `.gitmodules` (JUCE submodule entry)
- `scripts/bootstrap.sh` (lines 119-222: JUCE submodule init, patching)
- `scripts/check_deps.sh` (lines 100-105: JUCE submodule check)
- `scripts/juce-patches/` (1 patch file)

## Dependencies

This agent depends on ALL previous Phase 5 agents (01-04). After those complete:
- Zero `juce::` references in `src/vim/` (Agent 01)
- Zero `juce::` references in `src/ui/AppController.*` (Agent 02)
- Zero `#include <JuceHeader.h>` in non-gui source files (Agent 03)
- Zero `juce::` references in `src/Main.cpp` (Agent 04)

Verify this before proceeding:
```bash
grep -rn "juce::" src/ --include="*.h" --include="*.cpp" --include="*.mm" | grep -v "src/gui/" | grep -v "// .*juce::"
```
This should return zero hits (excluding gui/ and comments in dc/ headers).

## Deliverables

### 1. Delete src/gui/ directory

Delete the entire `src/gui/` directory tree (49 files):

```
src/gui/MainComponent.h, src/gui/MainComponent.cpp
src/gui/MainWindow.h, src/gui/MainWindow.cpp
src/gui/common/ColourBridge.h
src/gui/common/DremLookAndFeel.h, src/gui/common/DremLookAndFeel.cpp
src/gui/arrangement/ArrangementView.h, src/gui/arrangement/ArrangementView.cpp
src/gui/arrangement/AutomationLane.h, src/gui/arrangement/AutomationLane.cpp
src/gui/arrangement/Cursor.h, src/gui/arrangement/Cursor.cpp
src/gui/arrangement/MidiClipView.h, src/gui/arrangement/MidiClipView.cpp
src/gui/arrangement/TimeRuler.h, src/gui/arrangement/TimeRuler.cpp
src/gui/arrangement/TrackLane.h, src/gui/arrangement/TrackLane.cpp
src/gui/arrangement/WaveformView.h, src/gui/arrangement/WaveformView.cpp
src/gui/browser/BrowserPanel.h, src/gui/browser/BrowserPanel.cpp
src/gui/midieditor/NoteComponent.h, src/gui/midieditor/NoteComponent.cpp
src/gui/midieditor/PianoKeyboard.h, src/gui/midieditor/PianoKeyboard.cpp
src/gui/midieditor/PianoRollEditor.h, src/gui/midieditor/PianoRollEditor.cpp
src/gui/mixer/ChannelStrip.h, src/gui/mixer/ChannelStrip.cpp
src/gui/mixer/MeterComponent.h, src/gui/mixer/MeterComponent.cpp
src/gui/mixer/MixerPanel.h, src/gui/mixer/MixerPanel.cpp
src/gui/mixer/PluginSlotList.h, src/gui/mixer/PluginSlotList.cpp
src/gui/sequencer/PatternSelector.h, src/gui/sequencer/PatternSelector.cpp
src/gui/sequencer/StepButton.h, src/gui/sequencer/StepButton.cpp
src/gui/sequencer/StepGrid.h, src/gui/sequencer/StepGrid.cpp
src/gui/sequencer/StepSequencerView.h, src/gui/sequencer/StepSequencerView.cpp
src/gui/transport/TransportBar.h, src/gui/transport/TransportBar.cpp
src/gui/vim/VimStatusBar.h, src/gui/vim/VimStatusBar.cpp
```

Verify no non-gui code includes any gui/ headers:
```bash
grep -rn '#include.*gui/' src/ --include="*.h" --include="*.cpp" --include="*.mm" | grep -v "src/gui/"
```

### 2. CMakeLists.txt — Remove JUCE

Make these specific changes (in order):

**a) Remove JUCE subdirectory (line 12):**
Delete: `add_subdirectory(libs/JUCE)`

**b) Remove juce_add_gui_app (lines 117-123):**
Delete the entire block:
```cmake
juce_add_gui_app(DremCanvas
    PRODUCT_NAME "Drem Canvas"
    ...
)
```

Replace with a standard CMake executable:
```cmake
add_executable(DremCanvas)

# Set output directory to match previous JUCE layout
set_target_properties(DremCanvas PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/DremCanvas_artefacts/$<CONFIG>"
    OUTPUT_NAME "DremCanvas"
)
```

On macOS, create an app bundle:
```cmake
if(APPLE)
    set_target_properties(DremCanvas PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.drem.canvas"
        MACOSX_BUNDLE_BUNDLE_NAME "Drem Canvas"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "icon_1024.png"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/DremCanvas_artefacts/$<CONFIG>"
    )
endif()
```

**c) Remove juce_generate_juce_header (line 125):**
Delete: `juce_generate_juce_header(DremCanvas)`

**d) Remove JUCE compile definitions (lines 395-416):**
Remove all `JUCE_*` definitions from `target_compile_definitions`:
- `JUCE_WEB_BROWSER=0`
- `JUCE_USE_CURL=0`
- `JUCE_PLUGINHOST_VST3=1`
- `JUCE_DISPLAY_SPLASH_SCREEN=0`
- `JUCE_APPLICATION_NAME_STRING=...`
- `JUCE_APPLICATION_VERSION_STRING=...`
- `JUCE_PLUGINHOST_AU=1` (macOS)
- `JUCE_PLUGINHOST_AU=0` (Linux)
- `JUCE_ALSA=1` (Linux)
- `JUCE_JACK=0` (Linux)

Keep the Skia definitions (`SK_GANESH=1`, `SK_METAL=1`, `SK_VULKAN=1`).

**e) Remove JUCE link targets (lines 420-431):**
Remove all `juce::juce_*` entries from `target_link_libraries`:
```
juce::juce_audio_basics
juce::juce_audio_devices
juce::juce_audio_formats
juce::juce_audio_processors
juce::juce_audio_utils
juce::juce_core
juce::juce_data_structures
juce::juce_gui_basics
juce::juce_gui_extra
juce::juce_recommended_config_flags
juce::juce_recommended_warning_flags
```

Keep: `yaml-cpp::yaml-cpp`, `sdk`, `${SKIA_LIB}`, `PNG::PNG`, and all platform libs.

**f) No gui/ sources should be in target_sources:**
Verify the `target_sources` block does NOT list any `src/gui/` files. (The current
CMakeLists.txt doesn't list gui/ sources — they were removed during earlier migration
when the ui/ layer was added. But verify.)

### 3. Delete JUCE submodule

```bash
git rm libs/JUCE                    # removes submodule entry from index
rm -rf .git/modules/libs/JUCE      # remove cached module (if exists in this worktree)
```

Also remove the JUCE entry from `.gitmodules`. If JUCE is the only submodule,
delete `.gitmodules` entirely.

### 4. Delete scripts/juce-patches/

```bash
rm -rf scripts/juce-patches/
```

### 5. Update scripts/bootstrap.sh

Remove the entire JUCE submodule section (lines 119-222 — step `[2/5]`).
Renumber the remaining steps:
- `[1/5]` Check system prerequisites → `[1/4]` (same content)
- `[2/5]` JUCE submodule → **DELETE**
- `[3/5]` Skia → `[2/4]` Skia
- `[4/5]` CMake configure → `[3/4]` CMake configure
- `[5/5]` Done → `[4/4]` Done

Also update the header comment count (was "Takes any worktree from zero to buildable").

### 6. Update scripts/check_deps.sh

Remove the JUCE submodule check (lines 100-105):
```bash
# JUCE submodule                    ← DELETE this block
if [ -f "$PROJECT_ROOT/libs/JUCE/CMakeLists.txt" ]; then
    check "JUCE submodule" 0
else
    check "JUCE submodule (libs/JUCE/)" 1
fi
```

Optionally add checks for the new dependencies (PortAudio, libsndfile, RtMidi, VST3 SDK)
if they're not already checked. The VST3 SDK is fetched by CMake so no system check needed.

### 7. Remove juce:: comments from dc/ headers

The following dc/ headers have documentation comments referencing JUCE (these are
informational only — remove the `juce::` references from comments to complete cleanup):

- `src/dc/audio/AudioBlock.h` — line 22: "Overload for juce::AudioBuffer..."
- `src/dc/audio/DiskStreamer.h` — line 22: "Replaces juce::AudioFormatReaderSource..."
- `src/dc/audio/ThreadedRecorder.h` — line 23: "Replaces juce::AudioFormatWriter..."
- `src/dc/engine/AudioNode.h` — line 10: "replacing juce::AudioProcessor"
- `src/dc/midi/MidiSequence.h` — line 19: "Replaces juce::MidiMessageSequence"
- `src/dc/midi/MidiDeviceManager.h` — line 34: "Replaces juce::MidiInput / juce::MidiOutput..."

For each, either remove the comment or rephrase without mentioning JUCE. For example:
- "Replaces juce::AudioFormatReaderSource" → "Background disk reader with ring buffer."
- "replacing juce::AudioProcessor" → "Pure virtual audio processing interface."

## Final Verification

After all changes, run the complete verification suite:

```bash
# Zero JUCE symbols in source
grep -rn "juce::" src/ --include="*.h" --include="*.cpp" --include="*.mm"
# Expected: zero hits

# Zero JUCE includes
grep -rn "JuceHeader\|juce_" src/ --include="*.h" --include="*.cpp" --include="*.mm"
# Expected: zero hits

# No JUCE in CMake
grep -n "juce" CMakeLists.txt
# Expected: zero hits

# No gui/ directory
ls src/gui/ 2>&1
# Expected: "No such file or directory"

# No JUCE submodule
ls libs/JUCE/ 2>&1
# Expected: "No such file or directory"

# No JUCE patches
ls scripts/juce-patches/ 2>&1
# Expected: "No such file or directory"

# Clean build
rm -rf build/
cmake --preset release
cmake --build --preset release

# No JUCE symbols in binary (Linux)
nm build/DremCanvas_artefacts/Release/DremCanvas 2>/dev/null | grep -i juce
# Expected: zero hits

# Launch test (Linux)
./build/DremCanvas_artefacts/Release/DremCanvas &
sleep 2 && kill %1
```

## Scope Limitation

- Do NOT modify any non-gui source files in `src/` (except removing juce comments from dc/ headers)
- All functional code changes (VimEngine, AppController, Main.cpp) are handled by Agents 01-04
- This agent only deletes, removes, and adjusts build configuration
- If the pre-condition check (zero juce:: outside gui/) fails, STOP and report which files still have JUCE references

## Conventions

- Namespace: `dc`
- Build verification: `cmake --build --preset release`
- Use `git rm` for tracked files to maintain clean git history
