# Agent: Phase Plant E2E Test and Regression Test

You are working on the `feature/fix-scan-ui` branch of Drem Canvas, a C++17 DAW.
Your task is to create an end-to-end test that validates Phase Plant (yabridge-bridged
Kilohearts VST3 synth) can be scanned, loaded, its editor opened, and audio processed
without crashing. Also create a regression test documenting the yabridge scan blocking bug.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, test conventions)
- `tests/e2e/test_load_project.sh` (existing e2e pattern — fixture load + plugin validation)
- `tests/e2e/test_state_restore.sh` (e2e with `--process-frames` for audio validation)
- `tests/e2e/test_browser_scan.sh` (e2e for plugin scanning)
- `tests/e2e/test_scan_cold.sh` (e2e for spatial scan with editor open)
- `tests/e2e/e2e_display.sh` (shared helper: `run_with_display <timeout> <cmd> [args]`)
- `tests/fixtures/e2e-phase-plant/` (fixture created by Agent 01 — session.yaml + track-0.yaml)
- `tests/regression/issue_001_multi_yabridge_load.cpp` (existing yabridge regression test pattern)
- `tests/regression/README.md` (regression test template)
- `src/Main.cpp` (CLI flags: `--smoke`, `--load`, `--expect-tracks`, `--expect-plugins`, `--process-frames`, `--scan-plugin`, `--expect-plugin-name` if added by Agent 01)

## Dependencies

This agent depends on Agent 01 (Phase Plant Fixture and CLI Support). If the fixture
at `tests/fixtures/e2e-phase-plant/` doesn't exist yet, create minimal placeholder files
following the session.yaml + track-0.yaml pattern from `tests/fixtures/e2e-plugin-project/`.

## Deliverables

### New files (`tests/e2e/`)

#### 1. `test_phase_plant.sh`

End-to-end test that validates the full Phase Plant lifecycle. Follow existing patterns.

```bash
#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-phase-plant}"
```

**Prerequisite checks** (skip gracefully if not met):
- Phase Plant must be installed: `~/.vst3/yabridge/Kilohearts/Phase Plant.vst3`
- yabridge must be functional (the chainloader .so must exist inside the bundle)
- A display must be available (xvfb-run fallback via `run_with_display`)

**Test phases:**

1. **Load Phase Plant from fixture** — Use `--smoke --load <fixture> --expect-tracks 1 --expect-plugins 1`.
   This validates that the yabridge scan serialization fix works (Phase Plant is not blocked)
   and that `createPluginAsync()` successfully instantiates the plugin.
   Timeout: 60 seconds (yabridge loads require Wine bridge setup + 500ms settle).

2. **Process audio frames** — Add `--process-frames 30` to verify Phase Plant's `process()`
   runs on the audio thread without crashing. This catches VST3 lifecycle bugs
   (setupProcessing/setActive ordering).

3. **Open editor** (optional, if `--scan-plugin` is the mechanism) — If the fixture has
   exactly one plugin on track 0 slot 0, add `--scan-plugin 0 0 --expect-spatial-params-gt 0`
   to verify the editor opens and the spatial parameter finder works.
   Timeout should be 120 seconds total if editor scanning is included.

**Output**: `PASS: Phase Plant load + audio processing` on success.

**Important edge cases:**
- Phase Plant is a **large** yabridge plugin — Wine bridge initialization can take 5-10 seconds.
  Set timeouts generously (60s for load-only, 120s with editor).
- Use `XDG_CONFIG_HOME` isolation (temp dir) so ProbeCache starts fresh.
  Otherwise a previous crash could leave Phase Plant marked as `blocked`.

#### 2. `test_phase_plant_scan.sh`

Validates that Phase Plant appears in browser scan results (not blocked).

```bash
#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
```

**Prerequisite checks**: Phase Plant installed, display available.

**Test logic:**
- Use `XDG_DATA_HOME` + `XDG_CONFIG_HOME` isolation (fresh scan, no cache).
- Run: `--smoke --browser-scan --expect-known-plugins-gt 0`
- After the scan completes, verify Phase Plant is in the known plugins.
  The simplest way: check stderr output for `INFO: plugin scan found N plugins` where N
  includes yabridge plugins. If `--expect-known-plugins-gt` passes, Phase Plant was not
  blocked.
- For stronger validation: if `--expect-plugin-name` flag exists (from Agent 01),
  also pass `--expect-plugin-name "Phase Plant"` with `--browser-scan`.

**Output**: `PASS: Phase Plant found in browser scan`

### New files (`tests/regression/`)

#### 3. `issue_NNN_phase_plant_blocked.cpp`

Regression test documenting the bug where all yabridge plugins (including Phase Plant)
were marked as `blocked` after scanning.

Use the next available issue number. Check existing files:
```bash
ls tests/regression/issue_*.cpp | sort -t_ -k2 -n | tail -1
```

**Test structure** (Catch2):

```cpp
// Regression test: yabridge plugins marked blocked after scan
//
// Bug:  After merging feature/plugin-scan-bar and feature/fix-plugin-list-empty
//       independently, scanOneInProcess() loads yabridge bundles without
//       serialization. Wine bridge setup races cause SIGSEGV, marking all
//       yabridge plugins as blocked in ProbeCache. On subsequent launches,
//       only native plugins (Vital) appear.
//
// Fix:  Add yabridgeLoadMutex_ serialization to scanOneInProcess() with
//       500ms settle delay, matching VST3Host::getOrLoadModule() pattern.
//
// Verified by: tests/e2e/test_phase_plant.sh (loads Phase Plant end-to-end)

#include <catch2/catch_test_macros.hpp>
#include "dc/plugins/ProbeCache.h"
#include "dc/plugins/PluginScanner.h"
// ... additional includes as needed

TEST_CASE("yabridge plugins not blocked after serialized scan",
          "[regression][plugins]")
{
    // Test that ProbeCache does not mark a yabridge bundle as blocked
    // when scan completes successfully.
    // Use a mock/temp ProbeCache directory to avoid touching real cache.
    // ...
}
```

**Key assertions:**
- After a successful `scanOneInProcess()`, ProbeCache status is `safe`, not `blocked`
- The dead-man's-pedal file is cleaned up after successful scan
- If the pedal file exists on startup, the bundle IS blocked (crash recovery works)

Tag: `[regression][plugins]`

### Migration

#### 4. `CMakeLists.txt` — Add regression test source

Add the new regression test `.cpp` file to the test target's `target_sources`.
Follow the pattern used for existing regression tests:

```cmake
target_sources(dc_unit_tests PRIVATE
    tests/regression/issue_NNN_phase_plant_blocked.cpp
)
```

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line, `camelCase` methods
- Shell scripts: `set -euo pipefail`, `source e2e_display.sh`, skip gracefully if prerequisites missing
- Regression tests use Catch2 `TEST_CASE` with `[regression]` tag
- Regression test filename: `issue_NNN_short_description.cpp`
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset test`
- Run tests: `ctest --test-dir build-debug --output-on-failure -j$(nproc)`
