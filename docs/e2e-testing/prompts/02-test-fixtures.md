# Agent: Test Fixture Projects

You are working on the `feature/e2e-testing` branch of Drem Canvas, a C++17 DAW with
Skia/Vulkan rendering. Your task is creating session YAML fixture directories used by
E2E tests.

## Context

Read these specs before starting:
- `docs/e2e-testing/02-tier2-load-project.md` (fixture requirements, plugin choices)
- `docs/e2e-testing/03-tier3-visual-scan.md` (scan fixture requirements)
- `src/model/serialization/YAMLSerializer.cpp` (canonical YAML schema — see `emitSessionMeta()`, `emitTrack()`, `emitPluginChain()`)
- `tests/integration/test_session_roundtrip.cpp` (examples of session structure)

## YAML Schema Reference

Sessions are split across multiple files in a directory:

**session.yaml** — project metadata:
```yaml
drem_canvas_version: "0.1.0"
project:
  tempo: 120.0
  time_signature:
    numerator: 4
    denominator: 4
  sample_rate: 44100.0
  master_volume: 1.0
track_count: N
```

**track-N.yaml** — per-track data:
```yaml
track:
  name: "Track Name"
  colour: "#FF0000FF"
  mixer:
    volume: 0.75
    pan: 0.0
    mute: false
    solo: false
    armed: false
  clips: []
  plugins:
    - name: "PluginName"
      format: "VST3"
      manufacturer: "Manufacturer"
      unique_id: 12345
      file_or_identifier: "/path/to/plugin.vst3"
      state: ""
      enabled: true
```

## Deliverables

### New files

#### 1. `tests/fixtures/e2e-plugin-project/session.yaml`

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

#### 2. `tests/fixtures/e2e-plugin-project/track-0.yaml`

"Synth" track with Vital (native Linux VST3 instrument):

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
      state: ""
      enabled: true
```

Note: `unique_id: 0` is fine — the loader matches on `file_or_identifier` primarily.
Empty `state: ""` means plugins load with default presets.

#### 3. `tests/fixtures/e2e-plugin-project/track-1.yaml`

"FX" track with kHs Gain (yabridge-bridged effect):

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
      state: ""
      enabled: true
```

#### 4. `tests/fixtures/e2e-scan-project/session.yaml`

Single-track project for spatial scan testing:

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

#### 5. `tests/fixtures/e2e-scan-project/track-0.yaml`

"Scan Target" track with Vital for spatial scanning:

```yaml
track:
  name: "Scan Target"
  colour: "#FF66AAFF"
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
      state: ""
      enabled: true
```

## Scope Limitation

- Do NOT modify any C++ source files.
- Do NOT modify `CMakeLists.txt`.
- Do NOT create shell test scripts.
- Only create the YAML fixture files listed above.

## Conventions

- Match the YAML schema exactly as emitted by `YAMLSerializer::emitSessionMeta()` and
  `YAMLSerializer::emitTrack()`. Check the key names in `YAMLSerializer.cpp` if unsure.
- Colour values are hex ARGB strings (e.g., `#FF4488FF` = fully opaque).
- All clip/position values are in samples. Empty `clips: []` is valid.
- Build verification: `cmake --build --preset test`
