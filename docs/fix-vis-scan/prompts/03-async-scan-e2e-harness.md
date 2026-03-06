# Agent: Async Scan E2E Harness

You are working on the `feature/fix-vis-scan` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to add a `--browser-async-scan` CLI flag to the smoke test harness in `Main.cpp`, a shell test script, and a CTest registration — providing end-to-end coverage of the async browser scan UI lifecycle.

## Context

Read these files before starting:
- `src/Main.cpp` (two entry points: macOS at line 40, Linux at line 421; existing `--browser-scan` blocks at lines 346-372 and 725-751 — these use the **sync** `scanForPlugins()` API)
- `src/ui/AppController.h` (line 85-89 — existing accessors including `toggleBrowser()`, `getPluginManager()`; **Agent 02 adds** `getBrowserWidget()`)
- `src/ui/browser/BrowserWidget.h` (**Agent 01 adds** `wasProgressBarVisible()`, `isScanInProgress()`, `startAsyncScan()`)
- `tests/e2e/test_browser_scan.sh` (existing shell test pattern to follow)
- `tests/e2e/e2e_display.sh` (display helper: `run_with_display <timeout> <cmd> [args]`)
- `tests/CMakeLists.txt` (line 205-209 — existing `e2e.browser_scan` registration pattern)

## Dependencies

This agent depends on Agent 01 (BrowserWidget observability) and Agent 02 (AppController browser accessor). If those changes don't exist yet, create stub methods with the signatures from their respective prompts and implement against them.

Required methods from Agent 01 (`BrowserWidget`):
- `bool wasProgressBarVisible() const` — returns true if progress bar was visible during most recent scan
- `bool isScanInProgress() const` — returns true while async scan is running
- `void startAsyncScan()` — starts the async scan (already exists)

Required method from Agent 02 (`AppController`):
- `BrowserWidget* getBrowserWidget()` — returns pointer to the browser widget

## Deliverables

### Modified files

#### 1. `src/Main.cpp`

Changes are needed in **both** the macOS entry point (starting line 40) and the Linux entry point (starting line 421). The logic is identical in both.

**Declare the flag variable** — alongside the other flag declarations (after `bool browserScan = false;`):
```cpp
    bool browserAsyncScan = false;
```

**Parse the flag** — in the `for` loop that parses `argv` (after the `--browser-scan` case):
```cpp
        else if (arg == "--browser-async-scan")
            browserAsyncScan = true;
```

**Implement the test logic** — in the smoke-mode section, **after** the existing `browserScan` block and **before** the `expectKnownPluginsGt` standalone check. Insert:

```cpp
        // Async browser scan: exercises BrowserWidget::startAsyncScan() + tick() loop.
        // This validates the progress bar visibility lifecycle that the sync
        // --browser-scan flag cannot test.
        if (browserAsyncScan)
        {
            appController->toggleBrowser();
            appController->tick();

            auto* browser = appController->getBrowserWidget();
            if (browser == nullptr)
            {
                std::cerr << "FAIL: no BrowserWidget available\n";
                exitCode = 1;
            }
            else
            {
                browser->startAsyncScan();

                // Poll tick() until scan completes (up to ~120 seconds).
                // Each tick processes the message queue (delivers progress callbacks
                // from the scan thread) and BrowserWidget::tick() updates the
                // progress bar visibility — same flow as the real render loop.
                bool timedOut = true;
                for (int t = 0; t < 7500; ++t)  // 7500 * 16ms = 120s
                {
                    appController->tick();
                    if (! browser->isScanInProgress())
                    {
                        timedOut = false;
                        break;
                    }
                    std::this_thread::sleep_for (std::chrono::milliseconds (16));
                }

                if (timedOut)
                {
                    std::cerr << "FAIL: async scan timed out after 120s\n";
                    exitCode = 1;
                }
                else
                {
                    // Assert: progress bar was visible at some point during scan
                    if (! browser->wasProgressBarVisible())
                    {
                        std::cerr << "FAIL: progress bar was never visible during async scan\n";
                        exitCode = 1;
                    }
                    else
                    {
                        std::cerr << "INFO: progress bar was visible during scan (OK)\n";
                    }

                    // Drain a few more ticks to ensure completion handler ran
                    for (int i = 0; i < 5; ++i)
                        appController->tick();

                    int knownCount = static_cast<int> (
                        appController->getPluginManager().getKnownPlugins().size());
                    std::cerr << "INFO: async scan found " << knownCount << " plugins\n";

                    if (expectKnownPluginsGt >= 0 && knownCount <= expectKnownPluginsGt)
                    {
                        std::cerr << "FAIL: expected > " << expectKnownPluginsGt
                                  << " known plugins, got " << knownCount << "\n";
                        exitCode = 1;
                    }
                }

                // Close browser
                appController->toggleBrowser();
                appController->tick();
            }
        }
```

**Important**: The `expectKnownPluginsGt` standalone check (line 755 Linux, line 376 macOS) is guarded by `if (! browserScan && ...)`. Update the guard to also exclude `browserAsyncScan`:

```cpp
        if (! browserScan && ! browserAsyncScan && expectKnownPluginsGt >= 0)
```

This prevents a double-check of the plugin count when `--browser-async-scan` already validated it.

### New files

#### 2. `tests/e2e/test_browser_async_scan.sh`

Create this file (must be executable):

```bash
#!/usr/bin/env bash
# E2E: Async browser scan exercises the progress bar UI lifecycle.
#
# Validates that BrowserWidget::startAsyncScan() correctly shows the progress
# bar during scanning and hides it upon completion.  Uses isolated XDG
# directories so the scan starts with no cached state.
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"

# Check that at least one VST3 directory exists with plugins
HAS_PLUGINS=false
for dir in /usr/lib/vst3 "$HOME/.vst3"; do
    if [ -d "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
        HAS_PLUGINS=true
        break
    fi
done

if [ "$HAS_PLUGINS" = false ]; then
    echo "SKIP: no VST3 plugins found in standard paths"
    exit 0
fi

# Isolate both data and config to ensure fresh scan (no pluginList.yaml, no probeCache)
TMPDIR_DATA="$(mktemp -d)"
TMPDIR_CONFIG="$(mktemp -d)"
export XDG_DATA_HOME="$TMPDIR_DATA"
export XDG_CONFIG_HOME="$TMPDIR_CONFIG"
trap "rm -rf $TMPDIR_DATA $TMPDIR_CONFIG" EXIT

run_with_display 150 "$BINARY" \
    --smoke \
    --browser-async-scan \
    --expect-known-plugins-gt 0

echo "PASS: async browser scan exercised progress bar lifecycle"
```

### Modified files (continued)

#### 3. `tests/CMakeLists.txt`

After the `e2e.browser_scan` block (line 209), add:

```cmake
    add_test(NAME e2e.browser_async_scan
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_browser_async_scan.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.browser_async_scan PROPERTIES
        LABELS "e2e" TIMEOUT 180)
```

The 180-second CTest timeout gives 30 seconds of headroom beyond the 150-second `run_with_display` timeout.

## Scope Limitation

- Only modify `src/Main.cpp` and `tests/CMakeLists.txt`
- Only create `tests/e2e/test_browser_async_scan.sh`
- Do not modify `BrowserWidget`, `AppController`, `PluginManager`, `PluginScanner`, or any other source files
- Do not modify existing e2e test scripts
- Mirror the same logic in both the macOS and Linux entry points of `Main.cpp`

## Conventions

- Namespace: `dc`
- Spaces around operators, braces on new line for classes/functions
- `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
