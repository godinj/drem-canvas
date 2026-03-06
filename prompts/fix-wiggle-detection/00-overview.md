# Fix Wiggle Param Detection for Phase Plant

## Problem

Phase Plant's `IParameterFinder` returns valid spatial coordinates for UI controls, but its finder ParamIDs are **disjoint** from the enumerable `IEditController` ParamIDs. The scanner's three resolution phases all fail:

- **Direct lookup**: 0 matches (finder ParamIDs not in enumerable set)
- **Wiggle** (`setParamNormalized`): 0 matches (likely because controller rejects unknown ParamIDs)
- **performEdit snoop**: 0 matches (`findParameterAtPoint` alone doesn't trigger performEdit)

Result: 57 unmapped entries → all dropped → 0 hints for Phase Plant.

## Log Evidence

```
[SpatialScan] supportsParameterFinder=yes
[SpatialScan] 60 finder hits, 57 after filter (min 3), 0 direct, 0 wiggled, 0 snooped, 57 unmapped
```

## Investigation Order

1. **01-add-wiggle-debug-logging** — Instrument the wiggle phase to understand exactly why it fails. Does `getParamNormalized` return valid values for finder ParamIDs? Does `setParamNormalized` have any effect?

2. **02-investigate-yabridge-setparam-timing** — Determine if the failure is yabridge IPC timing, or if `setParamNormalized` fundamentally doesn't work with non-enumerable ParamIDs.

3. **03-implement-mouse-probe-wiggle** — If setParamNormalized can't work, implement mouse-probe resolution as the alternative. Inject XTest events at centroids from the IParameterFinder grid scan, catch performEdit callbacks.

## Key Files

- `src/plugins/ParameterFinderScanner.cpp` — grid scan + resolution phases
- `src/plugins/ParameterFinderScanner.h` — SpatialParamInfo struct
- `src/dc/plugins/PluginInstance.h` / `.cpp` — findRawParameterAtPoint, getController, popLastEdit
- `src/ui/pluginview/PluginViewWidget.cpp` — runSpatialScan orchestration, Phase 4 mouse probe, fallback grid
