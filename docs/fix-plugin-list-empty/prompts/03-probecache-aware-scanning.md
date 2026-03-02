# Agent: ProbeCache-Aware Scan Filtering

You are working on the `feature/fix-plugin-list-empty` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is to make the plugin scanner consult the ProbeCache
to skip blocked bundles and use mtime-based change detection to avoid rescanning unchanged
plugins.

## Context

Read these before starting:
- `docs/fix-plugin-list-empty/design.md` (Proposed Solution §3 and §4)
- `src/dc/plugins/PluginScanner.h/cpp` (full file — `scanAll()` loop, `scanOneForked()`, `scanOneInProcess()`)
- `src/dc/plugins/ProbeCache.h/cpp` (full file — `getStatus()`, `Status` enum, `Entry` struct with `mtime`)
- `src/dc/plugins/VST3Host.h/cpp` (`scanPlugins()` — where `setProbeCache()` is called, `loadDatabase()`, `saveDatabase()`, `knownPlugins_`)
- `src/dc/plugins/PluginDescription.h` (full file — `path` field used for matching)
- `src/plugins/PluginManager.h/cpp` (`scanForPlugins()` — calls `scanPlugins()` then `savePluginList()`)

## Dependencies

This agent depends on Agent 02 (In-Process Yabridge Scanning). If `PluginScanner::setProbeCache()`
and `scanOneInProcess()` don't exist yet, create them as described in Agent 02's deliverables
and implement against them.

## Deliverables

### Migration

#### 1. `src/dc/plugins/PluginScanner.h`

Add a method to accept a pre-existing plugin list for incremental scanning:

```cpp
// Add to public:
/// Set the previous known plugin list for incremental scan.
/// Plugins whose bundle mtime hasn't changed will be reused from this list
/// instead of being rescanned.
void setPreviousPlugins (const std::vector<PluginDescription>& plugins);

// Add to private:
std::vector<PluginDescription> previousPlugins_;
```

#### 2. `src/dc/plugins/PluginScanner.cpp`

**Add `setPreviousPlugins()`:**

```cpp
void PluginScanner::setPreviousPlugins (const std::vector<PluginDescription>& plugins)
{
    previousPlugins_ = plugins;
}
```

**Modify `scanAll()` to skip blocked and reuse unchanged:**

In the scan loop, before scanning each bundle:

1. **Skip blocked bundles**: If `probeCache_` is set and `probeCache_->getStatus(bundle)`
   returns `ProbeCache::Status::blocked`, skip the bundle entirely. Log it.

2. **Reuse unchanged plugins**: If the bundle's mtime matches the probe cache entry
   (i.e., `getStatus()` returns `safe`) AND a `PluginDescription` with the same `path`
   exists in `previousPlugins_`, reuse that description instead of rescanning. This
   avoids the expensive fork+load for plugins that haven't changed.

```cpp
for (int i = 0; i < total; ++i)
{
    const auto& bundle = allBundles[static_cast<size_t>(i)];
    auto pluginName = bundle.stem().string();

    if (progressCallback_)
        progressCallback_ (pluginName, i + 1, total);

    // Skip blocked bundles
    if (probeCache_ && probeCache_->getStatus (bundle) == ProbeCache::Status::blocked)
    {
        dc_log ("PluginScanner: skipping blocked: %s", pluginName.c_str());
        continue;
    }

    // Reuse cached description if bundle is safe and unchanged
    if (probeCache_ && probeCache_->getStatus (bundle) == ProbeCache::Status::safe)
    {
        bool reused = false;
        for (const auto& prev : previousPlugins_)
        {
            if (prev.path == bundle)
            {
                results.push_back (prev);
                dc_log ("PluginScanner: reused cached: %s", pluginName.c_str());
                reused = true;
                break;
            }
        }
        if (reused)
            continue;
    }

    // Full scan — yabridge in-process, native forked
    std::optional<PluginDescription> desc;

    if (VST3Module::isYabridgeBundle (bundle))
    {
        dc_log ("PluginScanner: yabridge bundle, scanning in-process: %s",
                pluginName.c_str());
        desc = scanOneInProcess (bundle);
    }
    else
    {
        desc = scanOneForked (bundle);
    }

    if (desc.has_value())
    {
        results.push_back (std::move (desc.value()));
        dc_log ("PluginScanner: scanned OK: %s", pluginName.c_str());
    }
    else
    {
        dc_log ("PluginScanner: failed to scan: %s", pluginName.c_str());
    }
}
```

Note: `ProbeCache::getStatus()` already checks mtime internally — it returns `unknown`
if the bundle's mtime has changed since the cache entry was written. So when `getStatus()`
returns `safe`, we know the bundle hasn't been modified. This makes the reuse safe.

#### 3. `src/dc/plugins/VST3Host.cpp`

Update `scanPlugins()` to pass the previous known plugins to the scanner for
incremental reuse:

```cpp
void VST3Host::scanPlugins (PluginScanner::ProgressCallback cb)
{
    if (cb)
        scanner_.setProgressCallback (std::move (cb));

    scanner_.setProbeCache (&probeCache_);
    scanner_.setPreviousPlugins (knownPlugins_);
    knownPlugins_ = scanner_.scanAll();
}
```

#### 4. `src/dc/plugins/ProbeCache.cpp`

Verify that `getStatus()` correctly returns `unknown` when the bundle's mtime differs
from the cached mtime. Read the implementation and confirm. If it doesn't check mtime,
add the check:

```cpp
ProbeCache::Status ProbeCache::getStatus (const std::filesystem::path& bundlePath) const
{
    auto it = entries_.find (bundlePath.string());
    if (it == entries_.end())
        return Status::unknown;

    // If bundle has been modified since we cached the result, treat as unknown
    auto currentMtime = getMtime (bundlePath);
    if (currentMtime != it->second.mtime)
        return Status::unknown;

    return it->second.status;
}
```

### New files

#### 5. `tests/unit/test_probecache_scan_filter.cpp`

Test the filtering behavior:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "dc/plugins/PluginScanner.h"
#include "dc/plugins/ProbeCache.h"
#include "dc/plugins/PluginDescription.h"

TEST_CASE ("PluginScanner reuses previous plugins for safe cached bundles", "[plugins]")
{
    // This is an integration-level test. On a system with no VST3 plugins,
    // scanAll() returns empty. The key contract to verify is that
    // setPreviousPlugins + setProbeCache are callable and don't crash.

    dc::PluginScanner scanner;
    std::vector<dc::PluginDescription> prev;
    scanner.setPreviousPlugins (prev);
    // scanAll() with no plugins installed should return empty, not crash
    auto results = scanner.scanAll();
    // Valid (possibly empty) vector
    (void) results;
}

TEST_CASE ("ProbeCache getStatus returns unknown for missing entry", "[plugins]")
{
    // Use a temp directory for the cache
    auto tmpDir = std::filesystem::temp_directory_path() / "dc-test-probecache";
    std::filesystem::create_directories (tmpDir);

    dc::ProbeCache cache (tmpDir);
    REQUIRE (cache.getStatus ("/nonexistent/plugin.vst3") == dc::ProbeCache::Status::unknown);

    // Cleanup
    std::filesystem::remove_all (tmpDir);
}
```

Add the new test file to `CMakeLists.txt` under the unit test target's `target_sources`.

## Scope Limitation

Do NOT modify the browser UI, AppController startup logic, or plugin instantiation code.
Only modify the scanner's `scanAll()` loop and the `VST3Host::scanPlugins()` call site.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Header includes use project-relative paths (e.g., `"dc/plugins/PluginScanner.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --build --preset test && ctest --test-dir build-debug --output-on-failure`
