# Agent: E2E Test for Plugin State Restoration

You are working on the `feature/plugin-loading-fixes-2` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting via yabridge (Wine bridge). Your task is adding an end-to-end test
that exercises the full plugin state restoration path — the code path that caused
deterministic SIGSEGV crashes in yabridge-bridged plugins (prompt 05).

## Context

Read these before starting:
- `src/Main.cpp` (both macOS and Linux entry points — the `--smoke` mode argv parsing and validation logic)
- `src/ui/AppController.cpp` (lines 1344-1526: `rebuildAudioGraph()` — the session restore flow: `createPluginSync` → `restorePluginState` → `setState()`)
- `src/plugins/PluginHost.cpp` (lines 32-45: `savePluginState()` / `restorePluginState()` — base64 encode/decode + `setState()`)
- `src/dc/plugins/PluginInstance.cpp` (lines 841-877: `getState()` — produces `[4 bytes componentSize][componentData][controllerData]`)
- `src/dc/plugins/PluginInstance.cpp` (lines 879-954: `setState()` — deactivation, state restore, reactivation)
- `tests/e2e/test_load_project.sh` (existing e2e test — loads plugins with empty state)
- `tests/e2e/e2e_display.sh` (shared display helper)
- `tests/fixtures/e2e-plugin-project/` (existing fixture — uses `state: ""`)

## Problem

The existing `test_load_project.sh` e2e test loads a project where all plugins have
`state: ""` (empty). This means `PluginHost::restorePluginState()` is never called,
`PluginInstance::setState()` is never entered, and the entire deactivation → state restore →
reactivation lifecycle (the code path that caused the SIGSEGV) is never exercised.

The bug fixed by prompt 05 is:

```
setState()  →  setProcessing(false)  →  setActive(false)  →  [restore state]
            →  setActive(true)  →  setProcessing(true)
                ↑ MISSING setupProcessing() before setActive(true)
                → yabridge plugin SIGSEGV on next process() call
```

This crash is deterministic with non-empty plugin state. We need an e2e test that:

1. Loads a project with **real saved plugin state** (non-empty base64 blob)
2. Runs enough ticks for the audio thread to call `process()` on the restored plugin
3. Verifies the app exits cleanly (no crash = test passes)

## Deliverables

### Step 1: State capture utility (`--capture-plugin-state`)

Add a new CLI flag to `Main.cpp` (both macOS and Linux entry points) that captures and
prints plugin state after loading a session.

**Argv parsing — add to the existing block:**

```cpp
bool capturePluginState = false;

// In the parsing loop:
else if (arg == "--capture-plugin-state")
    capturePluginState = true;
```

**Capture logic — add after the `--expect-plugins` validation block, inside the
`if (smokeMode)` section:**

```cpp
// Capture and print plugin state for test fixture generation
if (capturePluginState)
{
    auto& project = appController->getProject();
    for (int t = 0; t < project.getNumTracks(); ++t)
    {
        Track track (project.getTrack (t));
        auto& pluginChain = trackPluginChains[static_cast<size_t> (t)];

        for (int p = 0; p < static_cast<int> (pluginChain.size()); ++p)
        {
            if (pluginChain[static_cast<size_t> (p)].instance != nullptr)
            {
                auto stateStr = PluginHost::savePluginState (
                    *pluginChain[static_cast<size_t> (p)].instance);
                std::cout << "PLUGIN_STATE track=" << t
                          << " slot=" << p
                          << " state=" << stateStr << "\n";
            }
        }
    }
}
```

Note: `trackPluginChains` is a private member of `AppController`. You will need to add a
public accessor to expose it. Add to `src/ui/AppController.h`:

```cpp
/// Returns plugin chain info for the specified track (e2e test support).
const std::vector<PluginNodeInfo>& getTrackPluginChain (int trackIndex) const;
```

And implement in `AppController.cpp`:

```cpp
const std::vector<AppController::PluginNodeInfo>&
AppController::getTrackPluginChain (int trackIndex) const
{
    return trackPluginChains.at (static_cast<size_t> (trackIndex));
}
```

Then the capture logic in `Main.cpp` uses:

```cpp
if (capturePluginState)
{
    auto& project = appController->getProject();
    for (int t = 0; t < project.getNumTracks(); ++t)
    {
        auto& pluginChain = appController->getTrackPluginChain (t);
        for (int p = 0; p < static_cast<int> (pluginChain.size()); ++p)
        {
            if (pluginChain[static_cast<size_t> (p)].instance != nullptr)
            {
                auto stateStr = PluginHost::savePluginState (
                    *pluginChain[static_cast<size_t> (p)].instance);
                std::cout << "PLUGIN_STATE track=" << t
                          << " slot=" << p
                          << " state=" << stateStr << "\n";
            }
        }
    }
}
```

### Step 2: Process-frames flag (`--process-frames N`)

Add a flag that lets the e2e test run `N` audio callback cycles after session load,
verifying the audio thread survives processing restored plugins.

**Argv parsing:**

```cpp
int processFrames = 0;

// In the parsing loop:
else if (arg == "--process-frames" && i + 1 < argc)
    processFrames = std::atoi (argv[++i]);
```

**Execution logic — add after the `capturePluginState` block, inside `if (smokeMode)`:**

```cpp
// Run audio frames to exercise the process() path
if (processFrames > 0)
{
    for (int f = 0; f < processFrames; ++f)
    {
        appController->tick();
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
}
```

This is deliberately simple: `tick()` drains the message queue, and the real audio
processing happens on the PortAudio callback thread concurrently. Sleeping 10ms per
frame at 44.1kHz/512 samples means ~1 audio callback per tick — enough to exercise
`process()` on all plugins in the graph.

### Step 3: Test fixture with saved state

Create a new fixture directory `tests/fixtures/e2e-state-restore/` with plugin state
captured from a real session.

**`tests/fixtures/e2e-state-restore/session.yaml`:**

```yaml
drem_canvas_version: "0.1.0"
project:
  tempo: 120.0
  time_signature:
    numerator: 4
    denominator: 4
  sample_rate: 44100.0
  master_volume: 1.0
track_count: 2
```

**`tests/fixtures/e2e-state-restore/track-0.yaml`:**

```yaml
track:
  name: "Synth"
  colour: "#FF4488FF"
  mixer:
    volume: 0.8
    pan: 0.0
    mute: false
    solo: false
    armed: false
  clips: []
  plugins:
    - name: "Vital"
      format: "VST3"
      manufacturer: "Vital Audio"
      unique_id: 0
      file_or_identifier: "/usr/lib/vst3/Vital.vst3"
      state: "<CAPTURED_STATE>"
      enabled: true
```

**`tests/fixtures/e2e-state-restore/track-1.yaml`:**

```yaml
track:
  name: "FX"
  colour: "#FF88CCFF"
  mixer:
    volume: 1.0
    pan: 0.0
    mute: false
    solo: false
    armed: false
  clips: []
  plugins:
    - name: "kHs Gain"
      format: "VST3"
      manufacturer: "Kilohearts"
      unique_id: 0
      file_or_identifier: "~/.vst3/yabridge/Kilohearts/kHs Gain.vst3"
      state: "<CAPTURED_STATE>"
      enabled: true
```

**To populate `<CAPTURED_STATE>` values**, run the existing e2e-plugin-project fixture
with the new capture flag:

```bash
cmake --build --preset release
./build/DremCanvas_artefacts/Release/DremCanvas \
    --smoke \
    --load tests/fixtures/e2e-plugin-project \
    --capture-plugin-state 2>/dev/null
```

This will print lines like:

```
PLUGIN_STATE track=0 slot=0 state=AAAA...base64...
PLUGIN_STATE track=1 slot=0 state=BBBB...base64...
```

Copy the `state=` values into the corresponding `track-N.yaml` files. These are the
default-preset states — small blobs, typically 1-10 KB base64.

**IMPORTANT:** Write a helper script `tests/fixtures/e2e-state-restore/capture.sh` that
automates this:

```bash
#!/usr/bin/env bash
# Capture default plugin states for the e2e-state-restore fixture.
# Run after building: ./tests/fixtures/e2e-state-restore/capture.sh
set -euo pipefail

BINARY="${1:-./build/DremCanvas_artefacts/Release/DremCanvas}"
FIXTURE_DIR="$(dirname "$0")"
SOURCE_FIXTURE="tests/fixtures/e2e-plugin-project"

# Check prerequisites
for plugin in "/usr/lib/vst3/Vital.vst3" "$HOME/.vst3/yabridge/Kilohearts/kHs Gain.vst3"; do
    if [ ! -e "$plugin" ]; then
        echo "ERROR: plugin not found: $plugin"
        exit 1
    fi
done

# Capture state from the empty-state fixture
OUTPUT=$(timeout 30 "$BINARY" --smoke --load "$SOURCE_FIXTURE" --capture-plugin-state 2>/dev/null)

# Parse and inject into track YAMLs
while IFS= read -r line; do
    if [[ "$line" =~ PLUGIN_STATE\ track=([0-9]+)\ slot=([0-9]+)\ state=(.*) ]]; then
        track="${BASH_REMATCH[1]}"
        state="${BASH_REMATCH[3]}"
        track_file="$FIXTURE_DIR/track-${track}.yaml"

        if [ -f "$track_file" ]; then
            # Replace the state: "<CAPTURED_STATE>" placeholder
            sed -i "s|state: \"<CAPTURED_STATE>\"|state: \"${state}\"|" "$track_file"
            echo "Updated $track_file with ${#state} chars of state"
        fi
    fi
done <<< "$OUTPUT"

echo "Done. Verify with: cat $FIXTURE_DIR/track-*.yaml"
```

### Step 4: E2E test script

**`tests/e2e/test_state_restore.sh`:**

```bash
#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-state-restore}"

# Check plugin availability — skip if plugins are missing
check_plugin() {
    local path="$1"
    path="${path/#\~/$HOME}"
    if [ ! -e "$path" ]; then
        echo "SKIP: plugin not found: $path"
        exit 0
    fi
}

check_plugin "/usr/lib/vst3/Vital.vst3"
check_plugin "~/.vst3/yabridge/Kilohearts/kHs Gain.vst3"

# Check that fixture has real state (not placeholder)
if grep -q '<CAPTURED_STATE>' "$FIXTURE/track-0.yaml" 2>/dev/null; then
    echo "SKIP: fixture state not captured — run capture.sh first"
    exit 0
fi

# Load project with saved state, then process 50 audio frames (~500ms).
# If setState() lifecycle is broken (missing setupProcessing before
# setActive), the audio thread will SIGSEGV during process() and
# this test will fail with a non-zero exit / signal.
run_with_display 30 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 2 \
    --expect-plugins 2 \
    --process-frames 50

echo "PASS: plugin state restore + audio processing survived"
```

### Step 5: Register in CMakeLists.txt

Add the new e2e test to `tests/CMakeLists.txt` following the pattern used for
`test_load_project.sh`:

```cmake
add_test(
    NAME e2e_state_restore
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/e2e/test_state_restore.sh
            $<TARGET_FILE:DremCanvas>
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/e2e-state-restore
)
set_tests_properties(e2e_state_restore PROPERTIES
    LABELS "e2e"
    TIMEOUT 30
)
```

## What This Validates

This test exercises the exact code path that caused the SIGSEGV:

1. `loadSessionFromDirectory()` loads the YAML with non-empty `state:` fields
2. `rebuildAudioGraph()` calls `PluginHost::restorePluginState()` for each plugin
3. `restorePluginState()` base64-decodes the state and calls `PluginInstance::setState()`
4. `setState()` deactivates → restores → **must call setupProcessing()** → reactivates
5. Audio thread calls `PluginInstance::process()` on the reactivated plugin
6. If step 4 is broken (missing `setupProcessing()`), step 5 crashes with SIGSEGV

With empty `state: ""`, steps 2-5 are skipped entirely (`restorePluginState` returns early
on empty string). The existing `test_load_project.sh` never reaches the bug.

The `--process-frames 50` flag ensures the audio thread runs long enough (~500ms) to
call `process()` multiple times on each plugin, giving the crash ample opportunity to
manifest.

## Scope Limitation

Do NOT modify:
- `PluginInstance::setState()` — the fix is handled by prompt 05
- `PluginInstance::process()` — the bypass guard is handled by prompt 06
- `PluginHost::restorePluginState()` — the decode/call chain is correct
- Any existing e2e test — this is additive

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- E2E tests are shell scripts in `tests/e2e/` using `e2e_display.sh` helper
- Fixtures go in `tests/fixtures/`
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run full verification: `scripts/verify.sh`
