# Agent: Background Scan with Progress UI

You are working on the `feature/fix-plugin-list-empty` branch of Drem Canvas, a C++17 DAW
with Skia rendering and a custom widget toolkit. Your task is to run plugin scanning on a
background thread so it doesn't block the UI, and show scan progress in the BrowserWidget.

## Context

Read these before starting:
- `docs/fix-plugin-list-empty/design.md` (Proposed Solution §4 — "Background scan with progress")
- `src/plugins/PluginManager.h/cpp` (full file — `scanForPlugins()`, `getKnownPlugins()`, `savePluginList()`)
- `src/dc/plugins/PluginScanner.h` (`ProgressCallback` typedef, `setProgressCallback()`)
- `src/dc/plugins/VST3Host.h/cpp` (`scanPlugins(ProgressCallback)` — already accepts a progress callback)
- `src/ui/browser/BrowserWidget.h/cpp` (full file — `scanButton.onClick`, `refreshPluginList()`, `filterPlugins()`)
- `src/ui/AppController.cpp` (line 92: `loadPluginList`, auto-scan trigger, line 666: browser creation)
- `src/dc/foundation/message_queue.h` (if it exists — for posting results back to message thread)
- `src/dc/foundation/worker_thread.h` (if it exists — for background work)

Also check how other parts of the app post work back to the main thread. Look for
`dc::MessageQueue` or `dc::Timer` patterns in `src/ui/AppController.cpp`.

## Dependencies

This agent depends on Agent 01 (Auto-Scan on Empty) and Agent 02 (In-Process Yabridge Scanning).
If the auto-scan trigger in `AppController::initialise()` doesn't exist yet, add it as
described in Agent 01. If `PluginScanner::setProbeCache()` doesn't exist yet, create a
stub as described in Agent 02.

## Deliverables

### Migration

#### 1. `src/plugins/PluginManager.h`

Add async scan support:

```cpp
// Add to public:

/// Scan state for UI feedback
enum class ScanState { idle, scanning, complete };

/// Start scanning on a background thread. Progress and completion are
/// reported via callbacks. Call from message thread only.
void scanForPluginsAsync (
    std::function<void (const std::string& name, int current, int total)> onProgress,
    std::function<void ()> onComplete);

/// Get current scan state
ScanState getScanState() const { return scanState_; }

// Add to private:
std::atomic<ScanState> scanState_ { ScanState::idle };
```

#### 2. `src/plugins/PluginManager.cpp`

Implement `scanForPluginsAsync()`:

```cpp
void PluginManager::scanForPluginsAsync (
    std::function<void (const std::string& name, int current, int total)> onProgress,
    std::function<void ()> onComplete)
{
    if (scanState_ == ScanState::scanning)
        return;  // Already scanning

    scanState_ = ScanState::scanning;

    std::thread ([this, onProgress = std::move (onProgress),
                  onComplete = std::move (onComplete)] ()
    {
        vst3Host_.scanPlugins ([&onProgress] (const std::string& name, int current, int total)
        {
            if (onProgress)
                onProgress (name, current, total);
        });

        savePluginList (getDefaultPluginListFile());
        scanState_ = ScanState::complete;

        if (onComplete)
            onComplete();
    }).detach();
}
```

Note: The `onProgress` and `onComplete` callbacks are called from the background thread.
The BrowserWidget must handle thread-safe delivery to the UI. The simplest approach is
to store the progress state in atomics and let the next `tick()` / repaint pick it up.

Update the existing synchronous `scanForPlugins()` to also set state:

```cpp
void PluginManager::scanForPlugins()
{
    scanState_ = ScanState::scanning;
    vst3Host_.scanPlugins();
    savePluginList (getDefaultPluginListFile());
    scanState_ = ScanState::complete;
}
```

#### 3. `src/ui/browser/BrowserWidget.h`

Add scan progress state:

```cpp
// Add to private:
std::atomic<bool> scanInProgress_ { false };
std::atomic<int> scanCurrent_ { 0 };
std::atomic<int> scanTotal_ { 0 };
std::string scanCurrentPlugin_;  // Only accessed from message thread after copy
std::atomic<bool> scanResultReady_ { false };
```

Add a method to start the async scan and a tick method for polling progress:

```cpp
// Add to public:
/// Start async plugin scan with progress display
void startAsyncScan();

/// Called each frame to check for scan completion
void tick();
```

#### 4. `src/ui/browser/BrowserWidget.cpp`

**Update the scan button callback** to use async scanning:

```cpp
scanButton.onClick = [this]()
{
    startAsyncScan();
};
```

**Implement `startAsyncScan()`:**

```cpp
void BrowserWidget::startAsyncScan()
{
    if (scanInProgress_)
        return;

    scanInProgress_ = true;
    scanCurrent_ = 0;
    scanTotal_ = 0;
    scanResultReady_ = false;
    scanButton.setLabel ("Scanning...");
    repaint();

    pluginManager.scanForPluginsAsync (
        [this] (const std::string& name, int current, int total)
        {
            scanCurrent_ = current;
            scanTotal_ = total;
            // Note: name copy not thread-safe for display, but
            // scanCurrent_/scanTotal_ atomics are sufficient for progress
        },
        [this] ()
        {
            scanResultReady_ = true;
        });
}
```

**Implement `tick()`:**

```cpp
void BrowserWidget::tick()
{
    if (scanInProgress_ && scanResultReady_)
    {
        scanInProgress_ = false;
        scanResultReady_ = false;
        scanButton.setLabel ("Scan Plugins");
        refreshPluginList();
        repaint();
    }
    else if (scanInProgress_)
    {
        // Update button label with progress
        int cur = scanCurrent_;
        int tot = scanTotal_;
        if (tot > 0)
        {
            scanButton.setLabel ("Scanning " + std::to_string (cur) + "/" + std::to_string (tot));
            repaint();
        }
    }
}
```

#### 5. `src/ui/AppController.cpp`

**Wire the browser tick into the app tick loop.** Find the existing `tick()` method
in `AppController` and add a call to `browserWidget->tick()` if the browser is visible:

```cpp
// Inside AppController::tick() or the timer callback:
if (browserWidget)
    browserWidget->tick();
```

**Update the auto-scan at startup** to use async scanning if the browser is already
created, or keep synchronous if it happens before UI setup. Since the auto-scan happens
at line 92 (before browser creation at line 666), keep it synchronous — the user won't
see a blocked UI because no window is visible yet.

Check whether `scanButton` has a `setLabel()` method. If the button widget doesn't
support dynamic label changes, look at the button implementation in `src/gui/` or
`src/graphics/` and add `setLabel(const std::string&)` if needed.

### New files

#### 6. `tests/unit/test_async_scan.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "plugins/PluginManager.h"
#include <thread>
#include <chrono>

TEST_CASE ("PluginManager async scan state transitions", "[plugins]")
{
    dc::PluginManager pm;
    REQUIRE (pm.getScanState() == dc::PluginManager::ScanState::idle);

    SECTION ("synchronous scan transitions through scanning to complete")
    {
        pm.scanForPlugins();
        REQUIRE (pm.getScanState() == dc::PluginManager::ScanState::complete);
    }

    SECTION ("async scan sets state to scanning")
    {
        std::atomic<bool> done { false };

        pm.scanForPluginsAsync (
            nullptr,
            [&done] () { done = true; });

        // State should be scanning (or already complete if very fast)
        auto state = pm.getScanState();
        REQUIRE ((state == dc::PluginManager::ScanState::scanning
                  || state == dc::PluginManager::ScanState::complete));

        // Wait for completion (max 30 seconds for slow systems)
        for (int i = 0; i < 300 && ! done; ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (100));

        REQUIRE (done);
        REQUIRE (pm.getScanState() == dc::PluginManager::ScanState::complete);
    }
}
```

Add the new test file to `CMakeLists.txt` under the unit test target's `target_sources`.

## E2E Verification

After implementing async scan, verify that the auto-scan at startup still works
correctly. The synchronous auto-scan (from Agent 01) must not be broken by the
async changes. Run:

```bash
tests/e2e/test_auto_scan.sh ./build/DremCanvas_artefacts/Release/DremCanvas
```

This test launches with a fresh data directory and asserts plugins are discovered
at startup without `--browser-scan`. It should print `PASS` and exit 0.

Also verify the existing browser scan E2E test still passes (tests the explicit
scan path which now uses async):

```bash
tests/e2e/test_browser_scan.sh ./build/DremCanvas_artefacts/Release/DremCanvas
```

## Scope Limitation

Do NOT modify the scanner itself (`PluginScanner::scanAll`, `scanOneForked`, etc.).
Only add async plumbing in `PluginManager` and progress display in `BrowserWidget`.
Do NOT touch `VST3Host::getOrLoadModule()` or `ProbeCache` internals.

## Conventions

- Namespace: `dc` (core), `dc::ui` (UI layer)
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Header includes use project-relative paths (e.g., `"plugins/PluginManager.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --build --preset test && ctest --test-dir build-debug --output-on-failure`
