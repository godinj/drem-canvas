# Fix Empty Plugin List — Design Document

## Problem

When Drem Canvas launches, `pluginList.yaml` is empty (`plugins: []`) even though
`probeCache.yaml` contains 4 modules (3 safe, 1 blocked). Plugins are never
available in the browser or via `:plugin` command.

### Runtime Data

**probeCache.yaml** (populated from previous sessions):
```yaml
modules:
  ~/.vst3/yabridge/Kilohearts/Phase Plant.vst3:
    mtime: 1772112756
    status: safe
  ~/.vst3/yabridge/Kilohearts/kHs Gate.vst3:
    mtime: 1772112756
    status: safe
  /usr/lib/vst3/Vital.vst3:
    mtime: 1772064066
    status: safe
  ~/.vst3/yabridge/Kilohearts/kHs Gain.vst3:
    mtime: 0
    status: blocked
```

**pluginList.yaml** (empty):
```yaml
plugins:
  []
```

## Root Cause Analysis

The app has **two separate plugin discovery mechanisms** that don't coordinate:

1. **Probe Cache** (`ProbeCache`) — tracks whether a VST3 bundle is safe to load.
   Populated lazily when `VST3Host::getOrLoadModule()` is called. Persisted to
   `probeCache.yaml`.

2. **Plugin Scanner** (`PluginScanner`) — discovers plugin metadata (name, channels,
   manufacturer, etc.) by forking a child process and loading each VST3 bundle.
   Results stored in `knownPlugins_` and persisted to `pluginList.yaml`.

### The Gap

- **At startup**: `AppController::initialise()` calls `pluginManager.loadPluginList()`
  which reads `pluginList.yaml` into `knownPlugins_`. If the file is empty,
  `knownPlugins_` is empty. **No scan is triggered automatically.**

- **Scanning only happens when**:
  1. User clicks "Scan Plugins" in the browser widget
  2. App is launched with `--browser-scan` flag (E2E testing)

- **The scanner (`scanOneForked()`) forks a child process** that calls
  `VST3Module::load()` in-process. For **yabridge plugins**, the forked child
  **cannot** initialize the Wine bridge (it inherits the parent's process state
  but Wine IPC setup fails in the fork). This means `scanOneForked()` fails for
  all yabridge plugins, returning `std::nullopt`.

- **Result**: Native plugins (like Vital) should scan successfully, but yabridge
  plugins (Phase Plant, kHs Gate, kHs Gain) will fail the forked scan. However,
  probeCache shows these were previously loaded successfully in-process.

### Why Even Vital Shows Empty

The most likely reason is that the scan has never been triggered in normal app
startup. The user needs to either:
1. Click "Scan Plugins" in the browser
2. Launch with `--browser-scan`

Without either, the app loads an empty `pluginList.yaml` and never populates it.

## Proposed Solution

### 1. Auto-scan on first launch / when plugin list is empty

After `pluginManager.loadPluginList()`, if `knownPlugins_` is empty, trigger
an automatic scan. This ensures the plugin list is populated without requiring
the user to manually scan.

### 2. Skip yabridge bundles in forked scan, use ProbeCache metadata instead

The PluginScanner should be aware of yabridge bundles. For yabridge plugins:
- Skip the fork-based scan (it will always fail)
- Instead, load them in-process with pedal protection (like `getOrLoadModule()`
  does) to extract metadata
- Or: after the forked scan completes, do a second pass for yabridge bundles
  that failed, using in-process scanning with pedal protection

### 3. Consult ProbeCache during scan to skip blocked bundles

The scanner currently doesn't know about probe results. Pass ProbeCache to the
scanner (or have VST3Host coordinate) so that:
- Blocked modules are skipped during scan (no point fork-probing a known crasher)
- Safe modules can optionally use a faster scan path

### 4. Incremental scan with change detection

Compare bundle mtime against probeCache entries. If a bundle hasn't changed since
last successful scan, reuse the cached PluginDescription from pluginList.yaml
instead of re-scanning.

## Key Files

### Plugin System Core (dc::)
- `src/dc/plugins/PluginScanner.h/cpp` — Fork-based scanner, `scanAll()`, `scanOneForked()`, `scanOne()`
- `src/dc/plugins/VST3Host.h/cpp` — Coordinates scanner + probe cache + module loading
- `src/dc/plugins/VST3Module.h/cpp` — Low-level VST3 module loading, `isYabridgeBundle()` detection
- `src/dc/plugins/ProbeCache.h/cpp` — Bundle safety cache (safe/blocked/unknown)
- `src/dc/plugins/PluginDescription.h/cpp` — Plugin metadata struct

### Plugin System Integration
- `src/plugins/PluginManager.h/cpp` — High-level scan/save/load API
- `src/plugins/PluginHost.h/cpp` — Instance creation bridge
- `src/ui/AppController.cpp` — App init (line 92: `loadPluginList`), `:plugin` command wiring

### UI
- `src/ui/browser/BrowserWidget.h/cpp` — Browser panel with scan button and plugin list

### Entry Points
- `src/Main.cpp` — `--browser-scan` flag triggers scan (macOS ~line 322, Linux ~line 656)

### Tests
- `tests/` — E2E tests with `--browser-scan --expect-known-plugins-gt`

## Implementation Order

1. **Auto-scan at startup when plugin list is empty** — immediate fix for empty list
2. **In-process yabridge scanning** — make yabridge plugins discoverable
3. **ProbeCache-aware scanning** — skip blocked, fast-path safe modules
4. **Background scan with progress** — don't block startup, show scan progress in browser
5. **Incremental mtime-based caching** — avoid rescanning unchanged plugins
