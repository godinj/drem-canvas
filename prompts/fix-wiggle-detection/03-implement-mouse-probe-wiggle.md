# Implement Mouse-Probe Wiggle as Alternative Resolution

## Context

If `setParamNormalized(finderParamId)` doesn't work for Phase Plant's disjoint finder ParamIDs (because the controller doesn't recognize them), we need an alternative wiggle mechanism.

The existing Phase 4 mouse probe in `PluginViewWidget.cpp` (lines 241-320) already injects synthetic mouse clicks via XTest at centroid positions and catches `performEdit` callbacks via `popLastEdit()`. This works — but currently it only runs AFTER the grid scan, and only for entries that already have centroids.

## Task

Implement a **mouse-probe wiggle** as the primary resolution strategy for unmapped entries from the IParameterFinder grid scan. This replaces the current `setParamNormalized` wiggle in `ParameterFinderScanner::scan()`.

### Approach

Move the resolution of unmapped entries **out of** `ParameterFinderScanner::scan()` and into `PluginViewWidget::runSpatialScan()`, where we have access to the X11 input probe (`inputProbe`).

For each unmapped entry (`paramIndex == -1`) after the grid scan:
1. Use the entry's centroid `(centerX, centerY)` as the mouse probe target
2. Drain stale edit events: `while (plugin->popLastEdit().has_value()) {}`
3. Inject a synthetic mouse press+release at the centroid via `inputProbe`
4. Read `plugin->popLastEdit()` — if an edit event was generated, the `paramId` in the event tells us which enumerable parameter the control maps to
5. Look up the enumerable parameter index from that `paramId`

This is essentially the same as the existing Phase 4 mouse probe, but applied specifically to IParameterFinder entries that couldn't be resolved by direct lookup.

### Changes

1. **`src/plugins/ParameterFinderScanner.cpp`**: Remove or disable the `setParamNormalized` wiggle phase (lines 79-133). Keep the direct lookup phase. Keep the performEdit snoop phase (Phase 2). Keep the unmapped-entry drop filter.

2. **`src/ui/pluginview/PluginViewWidget.cpp`**: After `spatialScanner.scan()` returns, iterate unmapped entries and run mouse-probe resolution using `inputProbe` before the Phase 4 section. Alternatively, merge this into the existing Phase 4 logic.

### Important Considerations

- The mouse probe requires coordinate translation from native plugin coords to screen coords (the existing Phase 4 code already handles this via `editorBridge->getEditorPosition()`)
- XTest events need a small delay to propagate (the existing code uses 10ms)
- Only works on X11 (not XWayland without IRunLoop) — but that's fine, same constraint as existing Phase 4
- The `performEdit` snoop in `ParameterFinderScanner::scan()` (Phase 2, lines 146-177) already tries something similar but WITHOUT injecting mouse events — it just calls `findParameterAtPoint` which doesn't always trigger performEdit. The mouse probe is more reliable.

## Files

- `src/plugins/ParameterFinderScanner.cpp` — remove/disable setParamNormalized wiggle
- `src/ui/pluginview/PluginViewWidget.cpp` — add mouse-probe resolution for unmapped IParameterFinder entries

## Build & Test

```bash
cmake --build --preset release
```

Launch, open Phase Plant, press `f`. Check stderr for resolution counts. Success = non-zero "probed" count for Phase Plant.
