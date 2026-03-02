# Agent: Scan Flags & Tests

You are working on the `feature/e2e-testing` branch of Drem Canvas, a C++17 DAW with
Skia/Vulkan rendering. Your task is adding `--scan-plugin`, `--no-spatial-cache`, and
`--expect-spatial-params-gt` CLI flags to `Main.cpp`, plus shell-based E2E tests for
cold and warm spatial scanning.

## Context

Read these specs before starting:
- `docs/e2e-testing/03-tier3-visual-scan.md` (full execution plan, test matrix)
- `docs/e2e-testing/04-implementation-plan.md` (Phases F + G)
- `src/Main.cpp` (has `--smoke`, `--load`, `--expect-tracks`, `--expect-plugins` from Agents 01+03)
- `src/ui/AppController.h` (has `getProject()` from Agent 03 — need to add `getPluginViewWidget()`)
- `src/ui/pluginview/PluginViewWidget.h` (public API: `setPlugin()`, `runSpatialScan()`, `hasSpatialHints()`, `getSpatialResults()`)
- `src/plugins/SpatialScanCache.h` (`invalidate()` static method)
- `src/plugins/ParameterFinderScanner.h` (`SpatialParamInfo` struct)
- `src/model/Track.h` (`getPlugin()`, `getNumPlugins()`)
- `src/plugins/PluginHost.h` (`createPluginSync()`, `descriptionFromPropertyTree()`)

## Dependencies

This agent depends on Agent 03 (load flags). `--smoke`, `--load`, and
`AppController::getProject()` must already exist. If they don't, add them yourself
following `docs/e2e-testing/prompts/03-load-flags.md`.

## Deliverables

### Modified files

#### 1. `src/Main.cpp`

Extend argv parsing to handle `--scan-plugin`, `--no-spatial-cache`, and
`--expect-spatial-params-gt`.

**Argv parsing (extend existing block):**

```cpp
// Add to existing variables:
int scanTrack = -1;
int scanSlot = -1;
bool noSpatialCache = false;
int expectSpatialParamsGt = -1;

// Add to the for loop:
else if (arg == "--scan-plugin" && i + 2 < argc)
{
    scanTrack = std::atoi(argv[++i]);
    scanSlot = std::atoi(argv[++i]);
}
else if (arg == "--no-spatial-cache")
    noSpatialCache = true;
else if (arg == "--expect-spatial-params-gt" && i + 1 < argc)
    expectSpatialParamsGt = std::atoi(argv[++i]);
```

**Scan logic in the smoke-mode block:**

After session loading and track/plugin validation (but before teardown), add:

```cpp
// Run spatial scan if requested
if (scanTrack >= 0 && scanSlot >= 0)
{
    auto& project = appController->getProject();
    if (scanTrack >= project.getNumTracks())
    {
        std::cerr << "FAIL: scan track " << scanTrack << " out of range\n";
        exitCode = 1;
    }
    else
    {
        dc::Track track(project.getTrack(scanTrack));
        if (scanSlot >= track.getNumPlugins())
        {
            std::cerr << "FAIL: scan slot " << scanSlot << " out of range\n";
            exitCode = 1;
        }
        else
        {
            auto pluginNode = track.getPlugin(scanSlot);
            auto desc = dc::PluginHost::descriptionFromPropertyTree(pluginNode);

            // Get the plugin instance from the track's plugin chain
            // The plugin was already instantiated during loadSessionFromDirectory
            auto* pluginViewWidget = appController->getPluginViewWidget();
            if (pluginViewWidget == nullptr)
            {
                std::cerr << "FAIL: no PluginViewWidget available\n";
                exitCode = 1;
            }
            else
            {
                // Invalidate cache if requested
                if (noSpatialCache)
                {
                    auto fileOrId = pluginNode.getProperty("pluginFileOrIdentifier",
                                                            std::string{});
                    // Width/height: use 0,0 to let invalidate match any size,
                    // or call with the editor's actual size after setPlugin
                    dc::SpatialScanCache::invalidate(fileOrId, 0, 0);
                }

                // Open plugin editor and trigger scan
                appController->openPluginEditor(scanTrack, scanSlot);

                // Drain ticks to allow plugin editor to open
                for (int i = 0; i < 30; ++i)
                    appController->tick();

                pluginViewWidget->runSpatialScan();

                // Poll for scan completion (up to ~10 seconds)
                bool scanDone = false;
                for (int i = 0; i < 600 && !scanDone; ++i)
                {
                    appController->tick();
                    scanDone = pluginViewWidget->hasSpatialHints();
                    if (!scanDone)
                        std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }

                if (!scanDone)
                {
                    std::cerr << "FAIL: spatial scan timed out\n";
                    exitCode = 1;
                }
                else
                {
                    int paramCount = static_cast<int>(
                        pluginViewWidget->getSpatialResults().size());
                    std::cerr << "INFO: spatial scan found " << paramCount
                              << " parameters\n";

                    if (expectSpatialParamsGt >= 0 && paramCount <= expectSpatialParamsGt)
                    {
                        std::cerr << "FAIL: expected > " << expectSpatialParamsGt
                                  << " spatial params, got " << paramCount << "\n";
                        exitCode = 1;
                    }
                }
            }
        }
    }
}
```

**Required includes** (add if not present):
```cpp
#include <thread>    // std::this_thread::sleep_for
#include <chrono>
#include "plugins/SpatialScanCache.h"
#include "plugins/PluginHost.h"
```

#### 2. `src/ui/AppController.h`

Add a public getter for the plugin view widget:

```cpp
// In the public section, near getProject():
PluginViewWidget* getPluginViewWidget() { return pluginViewWidget.get(); }
```

Also make `openPluginEditor` public (it's currently private). Move its declaration
from the private section to the public section. Do NOT change its signature or
implementation.

### New files

#### 3. `tests/e2e/test_scan_cold.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

# Check plugin availability
if [ ! -e "/usr/lib/vst3/Vital.vst3" ]; then
    echo "SKIP: Vital.vst3 not found"
    exit 0
fi

# Use isolated config to guarantee empty cache
export XDG_CONFIG_HOME="$(mktemp -d)"
trap "rm -rf $XDG_CONFIG_HOME" EXIT

timeout 60 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --scan-plugin 0 0 \
    --no-spatial-cache \
    --expect-spatial-params-gt 3

echo "PASS: spatial scan (cold, no cache)"
```

Make executable: `chmod +x tests/e2e/test_scan_cold.sh`.

#### 4. `tests/e2e/test_scan_warm.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

# Check plugin availability
if [ ! -e "/usr/lib/vst3/Vital.vst3" ]; then
    echo "SKIP: Vital.vst3 not found"
    exit 0
fi

# Use isolated config for reproducibility
export XDG_CONFIG_HOME="$(mktemp -d)"
trap "rm -rf $XDG_CONFIG_HOME" EXIT

# First run populates the cache
timeout 60 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --scan-plugin 0 0

# Second run should hit cache and still produce results
timeout 15 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --scan-plugin 0 0 \
    --expect-spatial-params-gt 3

echo "PASS: spatial scan (warm, from cache)"
```

Make executable: `chmod +x tests/e2e/test_scan_warm.sh`.

#### 5. `tests/CMakeLists.txt` (append)

Add after the `e2e.load_project` test entry:

```cmake
    add_test(NAME e2e.scan_cold
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_scan_cold.sh
                $<TARGET_FILE:DremCanvas>
                ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-scan-project)
    set_tests_properties(e2e.scan_cold PROPERTIES
        LABELS "e2e" TIMEOUT 90)

    add_test(NAME e2e.scan_warm
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_scan_warm.sh
                $<TARGET_FILE:DremCanvas>
                ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-scan-project)
    set_tests_properties(e2e.scan_warm PROPERTIES
        LABELS "e2e" TIMEOUT 90)
```

## Scope Limitation

- Do NOT modify `PluginViewWidget`, `ParameterFinderScanner`, `SpatialScanCache`,
  or any other plugin infrastructure. Use the existing APIs as-is.
- The `openPluginEditor()` method already exists in `AppController.cpp` — just move
  its declaration from private to public in the header.
- The scan poll loop uses `sleep_for(16ms)` — this is acceptable in smoke mode only.
  Do NOT add sleep calls to the normal render loop.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Header includes use `<JuceHeader.h>` plus project-relative paths
- Build verification: `cmake --build --preset test`
