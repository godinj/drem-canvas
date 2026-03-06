# Agent: Spatial Scan Diagnostic Logging

You are working on the `feature/fix-vis-scan` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is adding diagnostic logging throughout the spatial scan → hint render chain so failures are visible at runtime.

## Context

Read these specs before starting:
- `CLAUDE.md` (build commands, conventions)
- `src/ui/pluginview/PluginViewWidget.cpp` (`runSpatialScan()`, `hasSpatialHints()`, `paint()` overlay hint block)
- `src/plugins/ParameterFinderScanner.cpp` (`scan()` — already has logging at end, but entry/exit missing)
- `src/ui/AppController.cpp` (`onQuerySpatialHintCount` lambda around line 337)
- `src/platform/linux/X11PluginEditorBridge.cpp` (`openEditor()` — compositor start result)

## Problem

When spatial scan hints don't appear, there's no runtime visibility into where the chain breaks. The chain has 6 decision points that can silently abort:

1. `runSpatialScan()` early return (editorBridge not open, currentPlugin null)
2. `supportsParameterFinder()` result (determines primary vs fallback path)
3. Fallback `runMouseProbeGridScan()` result count
4. `onQuerySpatialHintCount()` return value (0 = no spatial mode)
5. `computeCompositeGeometry()` validity (`isCompositing()` check)
6. `spatialScanner.getResults()` being empty at paint time

## Deliverables

### Modified files

#### 1. `src/ui/pluginview/PluginViewWidget.cpp`

Add `dc_log` calls at these locations (use `#include "dc/foundation/assert.h"` which provides `dc_log` — already included):

**In `runSpatialScan()`** (line 168):

- At entry, log the preconditions:
  ```cpp
  dc_log ("[SpatialScan] runSpatialScan: editorBridge=%s isOpen=%s plugin=%s",
          editorBridge ? "yes" : "no",
          (editorBridge && editorBridge->isOpen()) ? "yes" : "no",
          currentPlugin ? "yes" : "no");
  ```
- After the early return guard (line 170-171), the log above covers it since it fires before the return.
- After cache load succeeds (line 194-197), log:
  ```cpp
  dc_log ("[SpatialScan] loaded %d params from cache", static_cast<int> (cached.size()));
  ```
- After `supportsParameterFinder()` check (line 202), log:
  ```cpp
  dc_log ("[SpatialScan] supportsParameterFinder=%s", currentPlugin->supportsParameterFinder() ? "yes" : "no");
  ```
- After `runMouseProbeGridScan()` fallback (line 208), log the result count:
  ```cpp
  dc_log ("[SpatialScan] fallback grid scan: %d results", static_cast<int> (spatialScanner.getResults().size()));
  ```
- At the end (line 281-292), log the final state:
  ```cpp
  dc_log ("[SpatialScan] complete: %d results, compositing=%s",
          static_cast<int> (spatialScanner.getResults().size()),
          (editorBridge && editorBridge->isCompositing()) ? "yes" : "no");
  ```

**In `paint()`** overlay hint block (around line 466):

Add a one-time log when hints are requested but `geo.valid` is false. Use a `static bool` or the existing `spatialScanComplete` to avoid spamming:

```cpp
if (spatialHintMode && spatialScanComplete)
{
    auto geo = computeCompositeGeometry();
    if (! geo.valid)
    {
        // Log once per scan cycle (spatialScanComplete resets on new plugin)
        static bool loggedGeoInvalid = false;
        if (! loggedGeoInvalid)
        {
            dc_log ("[SpatialScan] overlay hints skipped: compositeGeometry invalid "
                    "(compositing=%s)",
                    (editorBridge && editorBridge->isCompositing()) ? "yes" : "no");
            loggedGeoInvalid = true;
        }
    }
    // ... rest of hint rendering
}
```

Note: replace the `static bool` with a member variable `bool loggedGeoWarning = false` on `PluginViewWidget` that resets in `setPlugin()` and `clearPlugin()`. This avoids the static state issue.

#### 2. `src/ui/pluginview/PluginViewWidget.h`

Add member variable:

```cpp
bool loggedGeoWarning = false;
```

Reset it to `false` in `setPlugin()` and `clearPlugin()` alongside the existing `spatialScanComplete = false` lines.

#### 3. `src/platform/linux/X11PluginEditorBridge.cpp`

In `openEditor()`, after `compositor->startRedirect()` (around line 49-51), log the result:

```cpp
dc_log ("[X11Bridge] compositor startRedirect: %s (window=0x%lx)",
        compositorActive ? "OK" : "FAILED",
        embeddedEditor->getXWindow());
```

### No new files

This prompt only modifies existing files.

## Scope Limitation

- Only add logging — do NOT fix any bugs or change control flow.
- Use `dc_log` (from `dc/foundation/assert.h`), not `std::cerr`.
- Keep log messages prefixed with `[SpatialScan]` or `[X11Bridge]` for grep-ability.
- Do not add logging in hot paths (per-frame loops). The `paint()` log uses the member guard to fire once.

## Conventions

- Namespace: `dc::ui`
- Coding style: spaces around operators, braces on new line for functions, `camelCase` methods
- `dc_log` uses printf-style formatting
- Build verification: `cmake --build --preset release`
