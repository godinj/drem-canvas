# Agent: Bounce CLI + E2E Routing Test

You are working on the `feature/fix-routing` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to add a `--bounce` CLI flag for offline audio rendering, create an e2e test fixture with MIDI data routed to Phase Plant, and write the e2e test script that validates non-silent audio output.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, testing)
- `src/Main.cpp` (Linux main function starting ~line 420: CLI flag parsing, smoke mode logic, how `--process-frames` and `--capture-plugin-state` work)
- `src/engine/BounceProcessor.h` and `src/engine/BounceProcessor.cpp` (existing offline bounce implementation: takes `AudioGraph&` + `BounceSettings`, renders to WAV)
- `src/ui/AppController.h` (public API: `getProject()`, `getAudioEngine()` which has `getGraph()`)
- `src/engine/AudioEngine.h` (`getGraph()` returns `dc::AudioGraph&`, `getSampleRate()`)
- `tests/e2e/test_phase_plant.sh` (existing Phase Plant e2e test pattern: prerequisite checks, XDG isolation, `run_with_display`)
- `tests/e2e/e2e_display.sh` (display helper: `run_with_display <timeout> <cmd> [args]`)
- `tests/fixtures/e2e-phase-plant/session.yaml` (YAML fixture format)
- `tests/fixtures/e2e-phase-plant/track-0.yaml` (track YAML with plugin, clips, mixer state)
- `tests/fixtures/e2e-phase-plant/capture.sh` (state capture script pattern)
- `tests/CMakeLists.txt` lines 177-236 (how e2e tests are registered: `add_test`, `LABELS "e2e"`, `TIMEOUT`)
- `src/model/Project.h` (IDs namespace: `IDs::tempo`, time signature access via Project)

## Deliverables

### Modified files

#### 1. `src/Main.cpp`

Add CLI flags to BOTH the macOS (line 55 area) and Linux (line 438 area) argument parsing blocks:

```cpp
std::string bouncePath;
int bounceBars = 0;
```

In the parsing loop, add:
```cpp
else if (arg == "--bounce" && i + 1 < argc)
    bouncePath = argv[++i];
else if (arg == "--bounce-bars" && i + 1 < argc)
    bounceBars = std::atoi (argv[++i]);
```

In BOTH smoke-mode sections (macOS ~line 254, Linux ~line 634), after the `processFrames` block, add bounce logic:

```cpp
if (! bouncePath.empty() && bounceBars > 0)
{
    auto& project = appController->getProject();
    double tempo = project.getTempo();
    double sr = appController->getAudioEngine().getSampleRate();

    // Time sig from project (default 4/4)
    int timeSigNum = 4;  // project.getTimeSigNumerator() if available
    int timeSigDen = 4;

    double beatsPerBar = 4.0 * timeSigNum / timeSigDen;
    double totalBeats = bounceBars * beatsPerBar;
    auto lengthInSamples = static_cast<int64_t> ((totalBeats / tempo) * 60.0 * sr);

    // Ensure transport is playing so MidiClipProcessor generates events
    appController->getTransportController().setPositionInSamples (0);
    appController->getTransportController().play();

    dc::BounceProcessor bouncer;
    dc::BounceProcessor::BounceSettings settings;
    settings.outputFile = bouncePath;
    settings.sampleRate = sr;
    settings.lengthInSamples = lengthInSamples;

    if (! bouncer.bounce (appController->getAudioEngine().getGraph(), settings))
    {
        std::cerr << "FAIL: bounce failed\n";
        exitCode = 1;
    }
}
```

You will need to include `"engine/BounceProcessor.h"` at the top of Main.cpp if not already included. Also check if `getTransportController()` is public on AppController — if not, the existing `transportController` member may need a public accessor. Read `src/ui/AppController.h` to verify.

### New files (`tests/fixtures/e2e-phase-plant-routing/`)

#### 2. session.yaml

```yaml
drem_canvas_version: "0.1.0"
project:
  tempo: 120.0
  time_signature:
    numerator: 4
    denominator: 4
  sample_rate: 44100.0
  master_volume: 1.0
track_count: 1
```

#### 3. track-0.yaml

MIDI track with Phase Plant plugin and a 4-bar MIDI clip containing quarter-note C4s.

```yaml
track:
  name: "Synth Routing Test"
  colour: "#FF4488FF"
  type: midi
  mixer:
    volume: 0.8
    pan: 0.0
    mute: false
    solo: false
    armed: false
  clips:
    - type: midi
      start_position: 0
      length: 352800
      notes:
        - { pitch: 60, velocity: 100, start_beat: 0.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 1.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 2.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 3.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 4.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 5.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 6.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 7.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 8.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 9.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 10.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 11.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 12.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 13.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 14.0, length_beats: 1.0 }
        - { pitch: 60, velocity: 100, start_beat: 15.0, length_beats: 1.0 }
  plugins:
    - name: "Phase Plant"
      format: "VST3"
      manufacturer: "Kilohearts"
      unique_id: 0
      file_or_identifier: "~/.vst3/yabridge/Kilohearts/Phase Plant.vst3"
      state: ""
      enabled: true
```

Note: `length: 352800` = 4 bars at 120 BPM, 44100 Hz = `(16 beats / 120 BPM) * 60 * 44100`. The 16 quarter notes cover 4 bars of 4/4.

**IMPORTANT**: Read `src/model/Track.cpp` and `src/model/MidiClip.cpp` to verify the exact YAML field names for MIDI clips and notes. The field names above are guesses — adapt them to match the actual serialization format used by the existing codebase. Look at `tests/integration/test_session_roundtrip.cpp` for a working YAML example with MIDI clips.

#### 4. capture.sh

Copy `tests/fixtures/e2e-phase-plant/capture.sh` and adapt the paths:

```bash
#!/usr/bin/env bash
# Capture Phase Plant plugin state for the routing fixture.
set -euo pipefail
source "$(dirname "$0")/../../e2e/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE_DIR="$(cd "$(dirname "$0")" && pwd)"

PLUGIN_PATH="$HOME/.vst3/yabridge/Kilohearts/Phase Plant.vst3"
if [ ! -e "$PLUGIN_PATH" ]; then
    echo "ERROR: Phase Plant not found: $PLUGIN_PATH"
    exit 1
fi

TMPFIXTURE=$(mktemp -d)
trap "rm -rf $TMPFIXTURE" EXIT

cp "$FIXTURE_DIR/session.yaml" "$TMPFIXTURE/"

for f in "$FIXTURE_DIR"/track-*.yaml; do
    basename=$(basename "$f")
    python3 -c "
import re, sys
content = open('$f').read()
content = re.sub(r'state: \"[^\"]*\"', 'state: \"\"', content)
open('$TMPFIXTURE/$basename', 'w').write(content)
"
done

OUTPUT=$(run_with_display 60 "$BINARY" --smoke --load "$TMPFIXTURE" --capture-plugin-state 2>/dev/null)

count=0
while IFS= read -r line; do
    if [[ "$line" =~ PLUGIN_STATE\ track=([0-9]+)\ slot=([0-9]+)\ state=(.*) ]]; then
        track="${BASH_REMATCH[1]}"
        state="${BASH_REMATCH[3]}"
        track_file="$FIXTURE_DIR/track-${track}.yaml"

        if [ -f "$track_file" ]; then
            state_file=$(mktemp)
            printf '%s' "$state" > "$state_file"
            python3 -c "
import re
state = open('$state_file').read()
track = open('$track_file').read()
track = re.sub(r'state: \"[^\"]*\"', 'state: \"' + state + '\"', track)
open('$track_file', 'w').write(track)
"
            rm -f "$state_file"
            echo "Updated $track_file with ${#state} chars of state"
            count=$((count + 1))
        fi
    fi
done <<< "$OUTPUT"

if [ "$count" -eq 0 ]; then
    echo "ERROR: no plugin states captured"
    exit 1
fi

echo "Done. Captured $count plugin state(s)."
```

### New test file (`tests/e2e/`)

#### 5. test_phase_plant_routing.sh

```bash
#!/usr/bin/env bash
# E2E: Phase Plant MIDI routing — verify patch produces non-silent audio
#
# Validates the full routing pipeline:
#   1. Load fixture with Phase Plant + MIDI clip
#   2. Bounce 4 bars to WAV (offline render)
#   3. Verify output is non-silent (patch was applied correctly)
#
# Prerequisites:
#   - Phase Plant installed at ~/.vst3/yabridge/Kilohearts/Phase Plant.vst3
#   - yabridge functional
#   - sox installed (for RMS analysis) — degrades gracefully if missing

set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-phase-plant-routing}"

# ── Prerequisite checks ──────────────────────────────────────────────

PHASE_PLANT_PATH="$HOME/.vst3/yabridge/Kilohearts/Phase Plant.vst3"

if [ ! -d "$PHASE_PLANT_PATH" ]; then
    echo "SKIP: Phase Plant not found: $PHASE_PLANT_PATH"
    exit 0
fi

CHAINLOADER="$PHASE_PLANT_PATH/Contents/x86_64-linux/Phase Plant.so"
if [ ! -f "$CHAINLOADER" ]; then
    echo "SKIP: yabridge chainloader not found: $CHAINLOADER"
    exit 0
fi

# ── Environment isolation ────────────────────────────────────────────

TMPDIR_CONFIG="$(mktemp -d)"
BOUNCE_OUT="$(mktemp --suffix=.wav)"
export XDG_CONFIG_HOME="$TMPDIR_CONFIG"
trap "rm -rf $TMPDIR_CONFIG; rm -f $BOUNCE_OUT" EXIT

# ── Test: load + bounce 4 bars ───────────────────────────────────────

run_with_display 90 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 1 \
    --expect-plugins 1 \
    --bounce "$BOUNCE_OUT" \
    --bounce-bars 4

# ── Verify output ────────────────────────────────────────────────────

if [ ! -f "$BOUNCE_OUT" ]; then
    echo "FAIL: bounce output not created"
    exit 1
fi

FILE_SIZE=$(stat -c%s "$BOUNCE_OUT")

# 4 bars at 120 BPM = 8 seconds stereo 24-bit WAV ≈ 1.4 MB minimum
if [ "$FILE_SIZE" -lt 100000 ]; then
    echo "FAIL: bounce output too small ($FILE_SIZE bytes)"
    exit 1
fi

# Check RMS is above noise floor (requires sox)
if command -v sox >/dev/null 2>&1; then
    RMS=$(sox "$BOUNCE_OUT" -n stat 2>&1 | grep "RMS.*amplitude" | head -1 | awk '{print $NF}')
    if python3 -c "import sys; sys.exit(0 if float('$RMS') > 0.001 else 1)" 2>/dev/null; then
        echo "PASS: Phase Plant routing — non-silent output (RMS=$RMS)"
    else
        echo "FAIL: Phase Plant routing — output is silent (RMS=$RMS)"
        exit 1
    fi
else
    echo "PASS: Phase Plant routing — bounce output created ($FILE_SIZE bytes, sox not available for RMS check)"
fi
```

### Build integration

#### 6. `tests/CMakeLists.txt`

Add after the existing Phase Plant e2e tests (~line 230):

```cmake
add_test(NAME e2e.phase_plant_routing
    COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_phase_plant_routing.sh
            $<TARGET_FILE:DremCanvas>
            ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-phase-plant-routing)
set_tests_properties(e2e.phase_plant_routing PROPERTIES
    LABELS "e2e" TIMEOUT 120)
```

## Scope Limitation

This agent does NOT modify `PluginInstance.cpp` or any plugin processing code. It only adds:
- CLI flags + bounce integration in Main.cpp
- Test fixture data
- E2E test script
- CMake test registration

The actual routing fixes (ProcessContext, IParameterChanges) are handled by other agents.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"engine/BounceProcessor.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
