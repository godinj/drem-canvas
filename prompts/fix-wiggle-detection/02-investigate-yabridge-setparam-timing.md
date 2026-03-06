# Investigate yabridge setParamNormalized Timing

## Context

Phase Plant runs under yabridge (Wine bridge). Every VST3 API call crosses a Wine IPC boundary. The wiggle detection in `ParameterFinderScanner::scan()` does:

```cpp
ctrl->setParamNormalized(finderParamId, nudged);
// immediately reads all enumerable params:
for (int i = 0; i < numParams; ++i) {
    double current = ctrl->getParamNormalized(plugin->getParameterId(i));
    // check if changed...
}
```

If `setParamNormalized` is async across the Wine bridge, the enumerable params may not have updated by the time we read them. This would explain 0 wiggle matches.

## Task

1. **Read yabridge source** to understand whether `setParamNormalized` is synchronous or async through the bridge. Check:
   - yabridge's VST3 `IEditController` proxy implementation
   - Whether `setParamNormalized` waits for the Wine-side response before returning

2. **If async, add a small delay** after `setParamNormalized` before reading params. Try:
   - `std::this_thread::sleep_for(std::chrono::milliseconds(5))` after the set
   - If that fixes it, determine the minimum viable delay
   - Consider batching: set all nudges first, wait once, then read all

3. **Alternative: use performEdit path instead.** The performEdit snoop (Phase 2 in the scanner) uses `findParameterAtPoint` which triggers plugin-side logic. Check if injecting a mouse event at the centroid would be more reliable than `setParamNormalized` for the wiggle approach.

4. **Check if `setParamNormalized` with a non-enumerable ParamID is even valid.** The VST3 spec says `IEditController::setParamNormalized` takes a ParamID — but does it only work for ParamIDs that the controller knows about (from `getParameterInfo`)? If Phase Plant's finder returns ParamIDs outside the controller's known set, `setParamNormalized` might silently fail.

## Key Hypothesis

The most likely failure mode: `setParamNormalized(finderParamId, ...)` silently fails because `finderParamId` is not in the controller's parameter info list. The controller only accepts ParamIDs it owns. The finder returns IDs from the view's internal parameter space which the controller doesn't recognize.

If this is the case, the wiggle approach (nudge finder ParamID → detect enumerable change) fundamentally cannot work through `setParamNormalized`. We'd need an alternative: either inject mouse events at the centroid, or use the view's own parameter handling.

## Files

- `src/plugins/ParameterFinderScanner.cpp` — wiggle phase implementation
- `src/dc/plugins/PluginInstance.h` / `.cpp` — `getController()`, `findRawParameterAtPoint()`
- yabridge source (if available locally, or check GitHub: github.com/robbert-vdh/yabridge)

## Verification

Run with Phase Plant open, press `f`, check stderr for wiggle debug output from prompt 01.
