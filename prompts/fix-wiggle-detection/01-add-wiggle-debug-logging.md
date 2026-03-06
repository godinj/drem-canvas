# Add Debug Logging to Wiggle Phase

## Context

In `src/plugins/ParameterFinderScanner.cpp`, the wiggle param detection phase (lines 79-133) is producing **0 matches** for Phase Plant. The log output shows:

```
[SpatialScan] 60 finder hits, 57 after filter (min 3), 0 direct, 0 wiggled, 0 snooped, 57 unmapped
```

Phase Plant has `IParameterFinder` but its finder ParamIDs are disjoint from its enumerable `IEditController` ParamIDs. The wiggle phase is supposed to resolve this by nudging the finder ParamID via `setParamNormalized` and detecting which enumerable param changed. It's not working — we need to understand why.

## Task

Add detailed debug logging to the wiggle phase in `ParameterFinderScanner::scan()` to capture:

1. **For each unmapped entry entering the wiggle loop:**
   - The finder ParamID being wiggled
   - The original value read via `ctrl->getParamNormalized(finderParamId)` — is it returning a meaningful value or 0.0?
   - The nudged value sent via `ctrl->setParamNormalized(finderParamId, nudged)`

2. **After the nudge, for each enumerable param:**
   - Log the first 5 params where `current != baseline` (even if below threshold) — we need to see if ANY params change at all, and by how much
   - Log whether `matchIdx` was found or not

3. **After the restore:**
   - Log the value after restore to confirm `setParamNormalized` is actually taking effect

Use `dc_log` (defined in `dc/foundation/assert.h` as `fprintf(stderr, ...)`). Gate verbose per-param logging behind the first 3 unmapped entries to avoid log spam.

## Key Questions to Answer from Logs

- Does `getParamNormalized(finderParamId)` return a valid value for Phase Plant's finder ParamIDs, or does it return 0.0 / default?
- Does `setParamNormalized(finderParamId, nudged)` actually change anything? (Check by re-reading after set)
- Do ANY enumerable params change after the nudge, even below threshold?
- Is the issue that yabridge's Wine IPC is async and the change hasn't propagated by the time we read?

## Files

- `src/plugins/ParameterFinderScanner.cpp` — add logging to wiggle phase (lines 79-133)

## Build & Test

```bash
cmake --build --preset release
```

Then launch the app, open Phase Plant, press `f`, and check stderr output.
