# Tier 3: Visual Scan (with and without Spatial Cache)

## Goal

Load a project with a plugin, open the plugin editor, trigger the spatial scan, and
verify results are produced. Test both cold (no cache) and warm (cached) paths.

## Why this is the hardest tier

The visual scan requires:

1. A real plugin editor window (X11 child window composited via `X11Compositor`).
2. `IParameterFinder::findParameterAtPoint()` to work (JUCE patches must be applied).
3. Optionally, `SyntheticInputProbe` (XTest mouse injection) for the mouse-probe
   phase.
4. A writable `~/.config/DremCanvas/spatial-cache/` directory.

## Execution plan

### Step 1 — Add `--scan-plugin <track> <slot>` flag

When passed alongside `--smoke --load`:

- After loading the project and draining a few ticks, call
  `pluginViewWidget->setPlugin(...)` for the specified track/slot.
- Call `pluginViewWidget->runSpatialScan()`.
- Wait for scan completion (poll `hasSpatialHints()` across a few tick cycles, with a
  timeout).
- Report the number of discovered spatial parameters.

Add `--expect-spatial-params-gt N` — pass if the scan found more than N parameters.

### Step 2 — Control the spatial cache

Add `--no-spatial-cache` flag:

- Before scanning, call `SpatialScanCache::invalidate()` for the plugin.
- Or: set a temporary `XDG_CONFIG_HOME` so the cache directory is empty.
- This forces the full grid scan + wiggle + mouse-probe pipeline.

Without the flag, the test runs with cache enabled. If a cache file exists from a
previous run, the scan returns instantly from cache.

### Test matrix

| Test | Cache state | What it exercises |
|------|------------|-------------------|
| `test_scan_cold` | `--no-spatial-cache` | Full `ParameterFinderScanner::scan()`, wiggle fallback, `SyntheticInputProbe`, cache write |
| `test_scan_warm` | Run after cold | `SpatialScanCache::load()`, hint label regeneration |

### Step 3 — Plugin selection for scan testing

Use **Vital** (`/usr/lib/vst3/Vital.vst3`):

- Native Linux VST3, no Wine layer.
- Has a rich editor with many controls.
- Supports `IParameterFinder` (if the JUCE VST3 hosting patches are applied).

Alternatively, **kHs Gain** is simpler (fewer parameters, faster scan, more
deterministic result count).

### Step 4 — Write the E2E tests

```bash
# tests/e2e/test_scan_cold.sh
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

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

```bash
# tests/e2e/test_scan_warm.sh
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

# First run populates the cache
export XDG_CONFIG_HOME="$(mktemp -d)"
trap "rm -rf $XDG_CONFIG_HOME" EXIT

timeout 60 xvfb-run -a "$BINARY" \
    --smoke --load "$FIXTURE" --scan-plugin 0 0

# Second run should hit cache and still produce results
timeout 15 xvfb-run -a "$BINARY" \
    --smoke --load "$FIXTURE" --scan-plugin 0 0 \
    --expect-spatial-params-gt 3

echo "PASS: spatial scan (warm, from cache)"
```

### Step 5 — XTest requirements

The mouse-probe phase uses `XTestFakeMotionEvent` / `XTestFakeButtonEvent`. Under
Xvfb:

- XTest extension is available by default.
- But there is no real window manager, so mouse injection might behave differently.
- The grid scan (Phase 3) and wiggle fallback (Phase 3b) should still work — they
  don't depend on X11 input.
- The mouse probe (Phase 4) may produce fewer results under Xvfb — this is
  acceptable; the `--expect-spatial-params-gt` threshold should be set low enough to
  pass with just the grid scan.

## What this validates

- Plugin editor creation and X11 compositing don't crash under Xvfb.
- `ParameterFinderScanner::scan()` completes without hanging.
- `IParameterFinder` grid scan produces spatial hits.
- Wiggle-based ParamID-to-index mapping works.
- `SpatialScanCache::save()` writes a valid YAML file.
- `SpatialScanCache::load()` restores results correctly.
- Hint label assignment produces non-empty labels.
- End-to-end flow from `--load` through `setPlugin` to `runSpatialScan`.
