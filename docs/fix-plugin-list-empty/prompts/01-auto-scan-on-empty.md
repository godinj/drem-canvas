# Agent: Auto-Scan on Empty Plugin List

You are working on the `feature/fix-plugin-list-empty` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is to trigger an automatic plugin scan at startup when
the known plugin list is empty, so users don't have to manually click "Scan Plugins".

## Context

Read these before starting:
- `docs/fix-plugin-list-empty/design.md` (Root Cause Analysis — "The Gap" section)
- `src/ui/AppController.cpp` (line 63: `initialise()`, line 92: `loadPluginList`, line 99-118: `:plugin` command wiring)
- `src/plugins/PluginManager.h/cpp` (full file — `scanForPlugins()`, `loadPluginList()`, `getKnownPlugins()`)
- `src/dc/plugins/VST3Host.h/cpp` (full file — `scanPlugins()`, `loadDatabase()`, `saveDatabase()`)
- `src/ui/browser/BrowserWidget.cpp` (line 23-27: scan button callback, line 35: `refreshPluginList()` in constructor)

## Deliverables

### Migration

#### 1. `src/ui/AppController.cpp`

After the existing `pluginManager.loadPluginList()` call at line 92, add a check:
if `pluginManager.getKnownPlugins().empty()`, call `pluginManager.scanForPlugins()`.

```cpp
// Load plugin list
pluginManager.loadPluginList (pluginManager.getDefaultPluginListFile());

// Auto-scan if the persisted list was empty (first launch or cleared cache)
if (pluginManager.getKnownPlugins().empty())
{
    dc_log ("AppController: plugin list empty, triggering auto-scan");
    pluginManager.scanForPlugins();
}
```

This ensures the very first app launch discovers plugins without user intervention.
Subsequent launches load the persisted `pluginList.yaml` and skip the scan.

#### 2. `src/ui/browser/BrowserWidget.cpp`

The `BrowserWidget` constructor calls `refreshPluginList()` at line 35 to populate
the list widget from `knownPlugins_`. Since `AppController::initialise()` now ensures
`knownPlugins_` is populated before the browser is created (line 666), no change is
needed here. Verify this is the case — the browser must be created **after** the
auto-scan completes in `initialise()`.

Check `src/ui/AppController.cpp` around line 666 where `browserWidget` is created.
If it's created inside `initialise()` after the plugin load/scan block, no change needed.
If it's created before, move the browser creation to after the scan.

### New files

#### 3. `tests/unit/test_auto_scan_trigger.cpp`

A unit test verifying that `PluginManager::scanForPlugins()` is callable and that
after scanning, `getKnownPlugins()` returns results (or at minimum doesn't crash).

Since this is a unit test that can't easily set up real VST3 bundles, test the
integration contract:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "plugins/PluginManager.h"

TEST_CASE ("PluginManager scanForPlugins populates known plugins", "[plugins]")
{
    dc::PluginManager pm;

    SECTION ("known plugins is empty before scan or load")
    {
        REQUIRE (pm.getKnownPlugins().empty());
    }

    SECTION ("scanForPlugins does not crash")
    {
        // This will scan real system VST3 paths. On CI with no plugins
        // installed, it should return 0 plugins without crashing.
        pm.scanForPlugins();
        // Just verify we get back a valid (possibly empty) vector
        auto& plugins = pm.getKnownPlugins();
        (void) plugins;  // no crash = pass
    }
}
```

Add the new test file to `CMakeLists.txt` under the unit test target's `target_sources`.

## E2E Verification

After implementing the auto-scan trigger, run the E2E auto-scan test to verify the
full user-visible behavior. This test launches the app with a fresh (empty) data
directory and asserts that plugins are discovered without `--browser-scan`:

```bash
tests/e2e/test_auto_scan.sh ./build/DremCanvas_artefacts/Release/DremCanvas
```

This test was created by Agent 05. If it doesn't exist yet, the E2E verification can
be deferred to post-merge, but the unit test in deliverable 3 must pass.

The test should print `PASS: auto-scan found plugins at startup` and exit 0. If it
fails, the auto-scan trigger is not working — check that `pluginManager.scanForPlugins()`
is called before the smoke exit path in `Main.cpp`.

## Scope Limitation

Do NOT modify the scanning logic itself (`PluginScanner`, `VST3Host::scanPlugins`).
Only wire the auto-scan trigger in `AppController::initialise()` and add the test.
Other agents handle scanner improvements.

## Conventions

- Namespace: `dc` (core), `dc::ui` (UI layer)
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Header includes use project-relative paths (e.g., `"plugins/PluginManager.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --build --preset test && ctest --test-dir build-debug --output-on-failure`
