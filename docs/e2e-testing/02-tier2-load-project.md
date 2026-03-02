# Tier 2: Load a Project with Plugins

## Goal

Launch the app, load a saved session containing plugin references, verify plugins
instantiate and the audio graph rebuilds.

## Prerequisites

- `--smoke` flag exists (Tier 1).
- Xvfb works.

## Execution plan

### Step 1 — Add `--load <path>` flag to `Main.cpp`

When `--load /path/to/session/` is passed alongside `--smoke`:

- After `appController->initialise()`, call
  `appController->loadSessionFromDirectory(path)`.
- Run a few `tick()` frames (plugins load asynchronously — allow the message queue to
  drain).
- Tear down and exit.

### Step 2 — Create a test fixture project

Place a minimal session directory in `tests/fixtures/e2e-plugin-project/`:

```
tests/fixtures/e2e-plugin-project/
├── session.yaml          # tempo=120, sampleRate=44100, 2 tracks
├── track-0.yaml          # "Synth" track with Vital.vst3 (instrument)
└── track-1.yaml          # "FX" track with kHs Gain.vst3 (effect)
```

Plugin choice rationale:

- **Vital** (`/usr/lib/vst3/Vital.vst3`) — native Linux VST3, no Wine needed,
  instrument plugin with editor.
- **kHs Gain** (`~/.vst3/yabridge/Kilohearts/kHs Gain.vst3`) — simple Kilohearts
  effect via yabridge, fast to load.

The YAML files reference these by their on-disk paths. Store the plugin state as empty
base64 (`""`) — plugins will load with default presets.

### Step 3 — Expose load success as an exit code

Add `--expect-tracks N` and `--expect-plugins N` flags. After loading:

```cpp
if (expectTracks >= 0 && project.getNumTracks() != expectTracks)
    return 1;  // fail: wrong track count
```

`--expect-plugins N` counts total instantiated plugin nodes across all tracks. This
turns the binary itself into a test oracle.

### Step 4 — Write the E2E test

```bash
# tests/e2e/test_load_project.sh
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-plugin-project}"

timeout 30 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 2 \
    --expect-plugins 2

echo "PASS: project load with plugins"
```

### Step 5 — Handle plugin availability gracefully

Plugins might not exist on every machine. Two strategies:

1. **Skip if missing:** The test script checks for plugin `.vst3` paths first and
   `exit 0` with a skip message if absent. Ctest marks it as `SKIP`.
2. **Stub plugin fixture:** Create a second fixture with no plugins (just empty
   tracks) as a fallback test that always runs. The plugin-loading fixture is gated
   behind a `HAS_PLUGINS` ctest label.

## What this validates

- `SessionReader` parses the fixture YAML correctly.
- `AppController::loadSessionFromDirectory` rebuilds the audio graph.
- `PluginHost::createPluginSync` loads real VST3 binaries (native + yabridge).
- Plugin state restoration doesn't crash (even with empty state).
- Track count and plugin chain integrity after load.
- Clean teardown with live plugin instances.
