# IParameterFinder Spatial Hints — Research & Design

## Overview

VST3's `IParameterFinder` interface allows a host to query which parameter
occupies any given pixel coordinate in a plugin's editor view. By grid-scanning
the editor surface, we can build a spatial map of parameter locations and render
vimium-style hint labels directly on the composited plugin image.

## IParameterFinder Interface

Defined in `pluginterfaces/vst/ivstplugview.h` (VST 3.0.2):

```cpp
class IParameterFinder : public FUnknown
{
public:
    virtual tresult PLUGIN_API findParameter (
        int32 xPos, int32 yPos, ParamID& resultTag) = 0;
};
```

- Coordinates are relative to the plug-in view origin (native/unscaled pixels)
- `resultTag` is a `Steinberg::Vst::ParamID` (`uint32`)
- Returns `kResultOk` when a parameter is found at (x, y)
- The interface is queried from the `IPlugView` via `queryInterface()`

## Grid Scan Strategy

1. Iterate over the editor surface at a configurable grid step (default 8px)
2. Call `findParameter(x, y)` at each grid point
3. Accumulate hit positions per ParamID into centroid accumulators
4. Compute centroids for each discovered parameter
5. Map ParamIDs to JUCE parameter indices (direct lookup → name match → wiggle)
6. Sort by spatial position (top-to-bottom rows, left-to-right within rows)
7. Assign hint labels using `VimEngine::generateHintLabel()`

Performance: 800x600 editor at 8px step = ~7,500 COM calls, <100ms on message
thread. Acceptable as a one-time cost at editor open time.

## Tiered Approach

1. **IParameterFinder available**: Render spatial hint labels on the composited
   plugin editor image. Labels appear on top of actual knobs/sliders.
2. **IParameterFinder unavailable**: Fall back to existing parameter list hints
   in the ParameterGridWidget (no regression).

Detection is via `queryInterface()` at editor creation time.

## Adoption Reality

IParameterFinder is optional in VST3. Adoption is mixed:
- JUCE-based plugins: JUCE's plugin client implements IParameterFinder by
  default (using `Component::getComponentAt()` → parameter lookup)
- Native VST3 plugins: Some implement it (Steinberg, u-he), many don't
- Wine-bridged plugins: Works through yabridge's COM proxying

The fallback to parameter list hints ensures the feature degrades gracefully.

## ParamID Mismatch Problem

### Symptom

For yabridge-bridged plugins (Phase Plant, kHs Gate, and likely other non-JUCE
VST3 plugins), the `IParameterFinder` returns ParamIDs that don't exist in the
`IEditController`'s enumerable parameter list. The direct `idToParamMap` lookup
returns null, and the name-based fallback finds no match either:

```
[SpatialScan] 64 finder params, 0 mapped to JUCE indices
[SpatialHint] resolved index=4 paramId=665 juceParamIndex=-1 name=
```

### Root Cause

These plugins use internal/aliased ParamIDs for their GUI controls that differ
from the ParamIDs exposed through `IEditController::getParameterInfo()`. The
finder ParamID space and the enumerable ParamID space are disjoint. JUCE's
`idToParamMap` only contains enumerable IDs, so finder IDs never match.

### Solution: Wiggle-Based Resolution

The controller internally maps finder ParamIDs to enumerable ParamIDs. When we
call `setParamNormalized(finderParamId, nudgedValue)`, the controller updates
the corresponding enumerable parameter's cached value. By reading all JUCE
parameter values before and after the nudge, we detect which one changed.

**Algorithm** (`resolveFinderParamByWiggle`):

1. Read baseline `getValue()` for all JUCE parameters (fast — reads from
   `cachedParamValues`, no COM calls)
2. Read original value: `ctrl->getParamNormalized(finderParamId)`
3. Nudge by +0.002 (or -0.002 if near 1.0):
   `ctrl->setParamNormalized(finderParamId, nudged)`
4. Re-read all JUCE param values, find the one that differs (threshold: 0.0005)
5. Restore original: `ctrl->setParamNormalized(finderParamId, original)`
6. Return the matching JUCE parameter index, or -1

**Design decisions:**

- Nudge amount (0.002) is small enough to be imperceptible but large enough to
  exceed floating-point noise
- Detection threshold (0.0005) catches real changes while ignoring rounding
- Original value is always restored, so the plugin state is unchanged after probing
- The wiggle pass only runs for params that failed the direct and name-based
  lookups, keeping the fast path for well-behaved plugins

### Resolution Pipeline

The scanner (`ParameterFinderScanner::scan`) uses a two-phase approach:

1. **Phase 1 — Direct resolution**: `resolveFinderParamIndex()` tries direct
   `idToParamMap` lookup, then falls back to querying the `IEditController` by
   ParamID and matching by name against JUCE parameters
2. **Phase 2 — Wiggle fallback**: For any params still unmapped after Phase 1,
   `resolveFinderParamByWiggle()` nudges the value and detects which JUCE
   parameter changes

Diagnostic output:
```
[SpatialScan] 64 finder params, 0 mapped to JUCE indices, 52 via wiggle
[WiggleResolve] finderParamId=665 -> juceIndex=12 name="Cutoff"
```

### Files Changed

| File | Change |
|------|--------|
| `src/plugins/VST3ParameterFinderSupport.h` | Added `resolveFinderParamIndex()` (pure virtual) and `resolveFinderParamByWiggle()` (virtual, default no-op) |
| `libs/JUCE/.../juce_VST3PluginFormat.cpp` | `VST3PluginWindow` implements both methods: direct/name lookup and wiggle detection |
| `libs/JUCE/.../juce_VST3PluginFormatImpl.h` | Added `getEditController()` accessor on `VST3PluginInstanceHeadless` |
| `src/plugins/ParameterFinderScanner.cpp` | Two-phase scan: direct resolution then wiggle fallback for unmapped params |

## Coordinate Considerations

- IParameterFinder uses native (unscaled) plugin coordinates
- Our compositor captures at native resolution, then scales for display
- Hint label positions must be transformed: `screenX = drawX + nativeX * scale`
- The composited image is anchored bottom-right in the panel area

## Alternatives Evaluated

| Approach | Status | Why Not |
|----------|--------|---------|
| CLAP `ext_gui` | No equivalent | CLAP has no parameter-at-position API |
| Accessibility APIs | Fragile | AT-SPI/IA2 support varies wildly across plugins |
| Computer Vision | Heavy | OCR/template matching is slow and unreliable |
| Mouse injection | Invasive | Simulating clicks to probe controls is destructive |
| AU parameter mapping | macOS only | AU has no spatial query API |

## Implementation

### Interface: `VST3ParameterFinderSupport`

Header-only abstract class that JUCE's `VST3PluginWindow` inherits from.
External code discovers it via `dynamic_cast<VST3ParameterFinderSupport*>(editor)`.
Uses `unsigned int` instead of `Steinberg::Vst::ParamID` to avoid leaking VST3
SDK headers.

### Scanner: `ParameterFinderScanner`

Performs the grid scan and produces a sorted list of `SpatialParamInfo` structs
with centroid positions, JUCE parameter indices, and assigned hint labels.

### JUCE Patch: `VST3PluginWindow`

Patches `VST3PluginWindow` to cache the `IParameterFinder*` at construction,
expose it through the `VST3ParameterFinderSupport` interface, and implement
three resolution strategies: direct `idToParamMap` lookup, IEditController
name matching, and wiggle-based value-change detection.

### Integration: `PluginViewWidget`

Runs the scanner at plugin open time, renders hint labels on the composited
editor image when spatial hint mode is active, and resolves hint selections
back to JUCE parameter indices.

## Phase 4: Mouse Probe Resolution

### Problem

For Phase Plant (kiloHearts), phases 1-3 all fail. The finder returns 64
ParamIDs that are "ghost parameters" — the IEditController accepts
`setParamNormalized()` (returns OK) but doesn't store the value (readback
is always 0). The 2174 controller params include only ~30 real ones (Bypass,
Lanes, Macros, Master). The finder ParamID space is completely disjoint from
the controller's enumerable ParamID space.

### Approach

Inject synthetic X11 mouse events at each unmapped param's centroid position
and intercept the plugin's `performEdit()` callback to discover the real
controller ParamID. This is how DAWs like Cubase implement "Learn" mode.

### Off-Screen Window Fix

The editor window lives at (-10000, -10000) for Wayland compositor capture.
XTest events use root-relative coordinates, so events at negative positions
never reach the plugin window on XWayland. Fix: call `moveWindow()` to
reposition the editor at (0, 0) before probing, then back to (-10000, -10000)
after. The window is briefly visible during probing.

### Multi-Pass Probing

Different controls respond to different mouse gestures. Phase 4 tries four
strategies in sequence, skipping already-mapped params between passes:

| Pass | ProbeMode  | Gesture                        | Phase Plant hits |
|------|------------|--------------------------------|------------------|
| 0    | dragUp     | Press + drag 10px up + release  | 14/64            |
| 1    | dragDown   | Press + drag 10px down + release | 3/50            |
| 2    | dragRight  | Press + drag 10px right + release | 0/47           |
| 3    | click      | Press + release (no drag)        | 0/47            |

**Result: 17 of 64 resolved.** All visible automatable knobs/sliders in
Phase Plant's default view: Macros 1-8, Mod Wheel, Lane volumes ×3, Lane
pans ×3, Master output.

### The Remaining 47 Params

IParameterFinder tags 47 additional GUI regions with ParamIDs, but no mouse
gesture triggers `performEdit`. These are likely module selectors, effect
slot buttons, dropdown menus, lane tabs, waveform displays, and routing
controls — interactive GUI elements that the plugin handles internally
without going through the VST3 performEdit path.

#### Tier 1: Cursor Teleport (no mapping needed)

When user selects a spatial hint for an unmapped param, warp the mouse cursor
to the param's centroid position so the user can manually interact with it.
The hint system becomes "navigate to control" rather than "automate this param."

- Already have centroid coords from IParameterFinder scan
- Use `XTestFakeMotionEvent` to move cursor (same mechanism as nudgePlugin)
- Mark unmapped hints differently in the overlay (dimmer color or "?" suffix)
- **Low effort, high value** — implement first

#### Tier 2: Name Discovery via IEditController

Query `editController->getParameterInfo()` using the finder's ParamID to get
a display name, even for ghost params. The controller might return names like
"Lane 1 Select" or "Filter Type" even if the param isn't in JUCE's map.

- Add `getParamName(finderParamId)` method to `VST3ParameterFinderSupport`
- Use the name for spatial hint labels instead of blank text
- **Low effort** — just a controller query, no mouse injection

#### Tier 3: Additional Probe Strategies

a. **Scroll wheel** — `XTestFakeButtonEvent(dpy, 4/5, ...)` (button 4 = scroll
   up, 5 = scroll down). Many knobs and dropdowns respond to scroll even if
   they ignore click+drag.

b. **beginEdit snoop** — some plugins call `beginEdit(paramId)` before
   `performEdit`. Snoop `beginEdit` to confirm a control is interactive and
   capture its real paramID, even if `performEdit` doesn't fire within the
   timing window.

c. **Double-click** — press-release-press-release. Toggle buttons and some
   value fields fire performEdit on double-click.

d. **Larger drag + longer timing** — 30px drag with 100-200ms wait. Some
   controls have large dead zones or yabridge IPC latency spikes.

e. **Right-click** — button 3 press. Some controls expose value editing or
   reset via right-click.

#### Tier 4: Classify Unmapped Params

After all probe strategies, remaining unmapped params should be classified:
- **Display-only**: waveform viewers, spectrum analyzers, modulation displays
- **Navigation**: module tabs, lane selectors (state changes, not automation)
- **Unknown**: keep the spatial hint for cursor teleport

### Files

| File | Change |
|------|--------|
| `src/platform/linux/X11MouseProbe.h/.cpp` | `sendMouseProbe()` with `ProbeMode` enum, `moveWindow()`, `findDeepestChild()` |
| `src/plugins/VST3ParameterFinderSupport.h` | Added `beginEditSnoop()`, `endEditSnoop()`, `resolveParamIDToIndex()` |
| `libs/JUCE/.../juce_VST3PluginFormat.cpp` | VST3PluginWindow implements snoop methods |
| `libs/JUCE/.../juce_VST3PluginFormatImpl.h` | `editSnoopActive`, `editSnoopParamID`, `totalPerformEditCount`, `snoopWindowCallCount` atomics; performEdit hook |
| `src/ui/pluginview/PluginViewWidget.cpp` | Phase 4 multi-pass integration, on-screen window move |
| `src/plugins/ParameterFinderScanner.h` | `getMutableResults()` for Phase 4 |
| `CMakeLists.txt` | Added `src/platform/linux/X11MouseProbe.cpp` |
