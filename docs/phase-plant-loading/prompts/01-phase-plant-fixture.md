# Agent: Phase Plant Fixture and CLI Support

You are working on the `feature/fix-scan-ui` branch of Drem Canvas, a C++17 DAW.
Your task is to create a test fixture and any needed CLI support for loading Phase Plant
(a yabridge-bridged Kilohearts VST3 synthesizer) end-to-end.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `tests/fixtures/e2e-plugin-project/` (existing fixture pattern — session.yaml + track-N.yaml)
- `tests/fixtures/e2e-state-restore/` (fixture with state capture + capture.sh script)
- `src/Main.cpp` (CLI flag handling — `--smoke`, `--load`, `--expect-tracks`, `--expect-plugins`, `--process-frames`, `--scan-plugin`, `--open-plugin-editor` if it exists)
- `src/ui/AppController.cpp` — `insertPluginOnTrack()`, `openPluginEditor()`, `loadSessionFromDirectory()`
- `src/dc/plugins/PluginDescription.h` — fields: name, manufacturer, path, uid, numInputChannels, numOutputChannels, hasEditor, acceptsMidi, producesMidi
- `src/dc/plugins/VST3Host.cpp` — `createInstance()`, `getOrLoadModule()` (yabridge mutex + settle)

## Plugin Details

Phase Plant is installed at:
```
~/.vst3/yabridge/Kilohearts/Phase Plant.vst3
```

It is a **yabridge-bridged VST3 instrument** (synthesizer):
- Bundle contains `Contents/x86_64-win/` (yabridge chainloader signature)
- `acceptsMidi: true`, `producesMidi: false`
- `hasEditor: true` (large UI, ~1000x700)
- Manufacturer: `Kilohearts`

## Deliverables

### New files (`tests/fixtures/e2e-phase-plant/`)

#### 1. `session.yaml`

Standard session file for a single-track project:

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

#### 2. `track-0.yaml`

Single MIDI track with Phase Plant loaded:

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
    - name: "Phase Plant"
      format: "VST3"
      manufacturer: "Kilohearts"
      unique_id: 0
      file_or_identifier: "~/.vst3/yabridge/Kilohearts/Phase Plant.vst3"
      state: ""
      enabled: true
```

#### 3. `capture.sh`

State capture script following the pattern in `tests/fixtures/e2e-state-restore/capture.sh`.
Checks that Phase Plant is installed, loads the fixture with `--capture-plugin-state`,
parses `PLUGIN_STATE` output lines, and injects base64 state into `track-0.yaml`.

### Migration

#### 4. `src/Main.cpp` — Add `--expect-plugin-name` CLI flag (if not already present)

Check if the existing CLI flags are sufficient. The current flags are:
- `--expect-plugins N` — checks total plugin count
- `--expect-tracks N` — checks total track count

If there is no way to verify a **specific plugin loaded by name**, add:

```cpp
// In the CLI parsing block:
else if (arg == "--expect-plugin-name" && i + 1 < argc)
    expectPluginName = argv[++i];

// In the smoke-mode validation block, after --expect-plugins check:
if (!expectPluginName.empty())
{
    bool found = false;
    auto& project = appController->getProject();
    for (int t = 0; t < project.getNumTracks() && !found; ++t)
    {
        auto& chain = appController->getTrackPluginChain(t);
        for (auto& slot : chain)
        {
            if (slot.plugin != nullptr && slot.description.name == expectPluginName)
            {
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        std::cerr << "FAIL: plugin '" << expectPluginName << "' not instantiated\n";
        exitCode = 1;
    }
}
```

Add this to **both** the macOS and Linux `main()` blocks (they mirror each other).

**Important**: Only add this flag if the existing `--expect-plugins` flag cannot distinguish
whether Phase Plant specifically loaded (vs. some other plugin). If `--expect-plugins 1` on
a single-plugin fixture is sufficient, skip this and note it in your output.

## Scope Limitation

- Do NOT modify the plugin scanning or loading code — that is handled by other prompts on this branch.
- Do NOT modify BrowserWidget, PluginScanner, ProbeCache, or VST3Host.
- Only touch `src/Main.cpp` for the CLI flag addition.
- All new files go in `tests/fixtures/e2e-phase-plant/`.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Shell scripts: `set -euo pipefail`, source `e2e_display.sh` for display handling
- Build verification: `cmake --build --preset release`
