# Agent: Probe Cache Unblock Mechanism

You are working on the `feature/plugin-loading-fixes` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is adding the ability to retry and unblock plugins
that were permanently marked as blocked by the ProbeCache crash-detection system.

## Context

Read these before starting:
- `src/dc/plugins/ProbeCache.h` (full file — `Status` enum, `setStatus`, `getStatus`, dead-man's-pedal API)
- `src/dc/plugins/ProbeCache.cpp` (full file — `load()` crash recovery, `save()` YAML format, `getMtime()`)
- `src/dc/plugins/VST3Host.cpp` (line ~167: `getOrLoadModule()` — where blocked status causes early return with `nullptr`)
- `src/dc/plugins/VST3Host.h` (public API — `getProbeCache()` if it exists, or how to access the cache)
- `tests/unit/plugins/test_probe_cache.cpp` (existing tests — extend these)

## Problem

Once `ProbeCache::setStatus(path, Status::blocked)` is called (either from a dead-man's-pedal
crash recovery or an explicit probe failure), the plugin is permanently skipped in
`VST3Host::getOrLoadModule()`. There is no API to:

1. List which plugins are blocked
2. Reset a specific blocked plugin to `unknown` (retry)
3. Reset all blocked plugins (bulk retry)
4. Check if a blocked plugin's binary has been updated since it was blocked

The user sees `"ProbeCache: previous load of kHs Gate.vst3 crashed — marking blocked"` but
has no way to retry after fixing the plugin installation or updating yabridge.

## Deliverables

### Modified files

#### 1. `src/dc/plugins/ProbeCache.h`

Add three public methods:

```cpp
/// Reset a blocked module to unknown, allowing retry on next load.
void resetStatus (const std::filesystem::path& bundlePath);

/// Return all paths currently marked as blocked.
std::vector<std::filesystem::path> getBlockedPlugins() const;

/// Reset ALL blocked entries to unknown.
void resetAllBlocked();
```

#### 2. `src/dc/plugins/ProbeCache.cpp`

Implement the three methods:

- `resetStatus`: call `setStatus(bundlePath, Status::unknown)`. Simple wrapper with a
  descriptive name. Also clear any leftover pedal file for that path.
- `getBlockedPlugins`: iterate `entries_`, collect paths where `status == Status::blocked`.
  Return as `std::vector<std::filesystem::path>`.
- `resetAllBlocked`: iterate `entries_`, set all `blocked` to `unknown`, then `save()`.

#### 3. `src/dc/plugins/VST3Host.h` / `src/dc/plugins/VST3Host.cpp`

Expose the probe cache so callers (UI, CLI) can access unblock functionality:

```cpp
// In VST3Host.h, add public accessor:
ProbeCache& getProbeCache() { return probeCache_; }
const ProbeCache& getProbeCache() const { return probeCache_; }
```

If `probeCache_` is not already a member of `VST3Host`, check how the cache is accessed
in `getOrLoadModule()` and ensure the accessor returns the same instance.

Also add a convenience method that retries loading a blocked plugin:

```cpp
/// Reset a blocked plugin and attempt to load it again.
/// Returns the loaded module, or nullptr if it still fails.
std::shared_ptr<VST3Module> retryBlockedModule (const std::filesystem::path& bundlePath);
```

Implement `retryBlockedModule`:
1. Call `probeCache_.resetStatus(bundlePath)`
2. Call `probeCache_.save()`
3. Call `getOrLoadModule(bundlePath)` to re-attempt the load
4. Return the result (module or nullptr if it fails again)

#### 4. `tests/unit/plugins/test_probe_cache.cpp`

Add test sections to the existing test file:

```cpp
SECTION ("resetStatus clears blocked entry")
{
    cache.setStatus (pluginA, ProbeCache::Status::blocked);
    REQUIRE (cache.getStatus (pluginA) == ProbeCache::Status::blocked);

    cache.resetStatus (pluginA);
    REQUIRE (cache.getStatus (pluginA) == ProbeCache::Status::unknown);
}

SECTION ("getBlockedPlugins returns only blocked entries")
{
    cache.setStatus (pluginA, ProbeCache::Status::safe);
    cache.setStatus (pluginB, ProbeCache::Status::blocked);
    cache.setStatus (pluginC, ProbeCache::Status::blocked);

    auto blocked = cache.getBlockedPlugins();
    REQUIRE (blocked.size() == 2);
    // Verify both pluginB and pluginC are in the list
}

SECTION ("resetAllBlocked clears all blocked entries")
{
    cache.setStatus (pluginA, ProbeCache::Status::blocked);
    cache.setStatus (pluginB, ProbeCache::Status::blocked);
    cache.setStatus (pluginC, ProbeCache::Status::safe);

    cache.resetAllBlocked();

    REQUIRE (cache.getStatus (pluginA) == ProbeCache::Status::unknown);
    REQUIRE (cache.getStatus (pluginB) == ProbeCache::Status::unknown);
    REQUIRE (cache.getStatus (pluginC) == ProbeCache::Status::safe);  // safe is untouched
}

SECTION ("resetStatus also clears pedal for that path")
{
    cache.setPedal (pluginA);
    REQUIRE (cache.checkPedal().has_value());

    cache.resetStatus (pluginA);
    REQUIRE_FALSE (cache.checkPedal().has_value());
}
```

Use the existing test fixture variables (`pluginA`, `pluginB`, etc.) — read the test file
to see how they're defined.

## Scope Limitation

Do NOT build any UI for unblocking plugins in this prompt. Focus on the API layer only
(ProbeCache methods + VST3Host accessor). A future prompt will add the UI (plugin browser
"Retry" button, settings panel).

Do NOT modify `PluginInstance`, `AppController`, or `PluginHost`.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Run verification: `scripts/verify.sh`
