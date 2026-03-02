# Agent: In-Process Yabridge Scanning

You are working on the `feature/fix-plugin-list-empty` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting via yabridge (Wine bridge). Your task is to make yabridge plugins
discoverable during scanning by loading them in-process instead of forking, since forked
child processes cannot initialize the Wine bridge.

## Context

Read these before starting:
- `docs/fix-plugin-list-empty/design.md` (Proposed Solution §2 — "Skip yabridge bundles in forked scan")
- `src/dc/plugins/PluginScanner.h/cpp` (full file — `scanAll()`, `scanOneForked()`, `scanOne()`, `findBundles()`)
- `src/dc/plugins/VST3Module.h/cpp` (full file — `isYabridgeBundle()`, `load()`, `probeModuleSafe()`)
- `src/dc/plugins/VST3Host.cpp` (lines 149-265: `getOrLoadModule()` — shows yabridge-aware loading with pedal protection and settle delay)
- `src/dc/plugins/ProbeCache.h/cpp` (full file — `setPedal()`, `clearPedal()`, `getStatus()`, `setStatus()`, `Status` enum)

## Deliverables

### Migration

#### 1. `src/dc/plugins/PluginScanner.h`

Add a `ProbeCache*` member and setter so the scanner can use pedal protection during
in-process yabridge scans:

```cpp
// Add to public:
/// Set the probe cache for yabridge in-process scanning.
/// Must be called before scanAll() if yabridge plugins are present.
void setProbeCache (ProbeCache* cache);

// Add to private:
ProbeCache* probeCache_ = nullptr;

/// Scan a yabridge bundle in-process with pedal protection.
/// Returns nullopt if the load fails or the module has no audio effect class.
std::optional<PluginDescription> scanOneInProcess (
    const std::filesystem::path& bundlePath);
```

#### 2. `src/dc/plugins/PluginScanner.cpp`

**Add `setProbeCache()`:**

```cpp
void PluginScanner::setProbeCache (ProbeCache* cache)
{
    probeCache_ = cache;
}
```

**Add `scanOneInProcess()`:**

This method loads the module directly (no fork) with dead-man's-pedal crash protection
via `ProbeCache`. It reuses the existing `scanOne()` logic but wraps it with pedal
set/clear:

```cpp
std::optional<PluginDescription> PluginScanner::scanOneInProcess (
    const std::filesystem::path& bundlePath)
{
    // Use pedal protection if ProbeCache is available
    if (probeCache_)
        probeCache_->setPedal (bundlePath);

    auto result = scanOne (bundlePath);

    if (probeCache_)
        probeCache_->clearPedal();

    return result;
}
```

Note: `scanOne()` already calls `VST3Module::load(bundlePath)` which defaults to
`skipProbe = false`. For yabridge bundles being scanned in-process, we want to skip
the fork probe (it will fail) and load directly. Modify the `scanOneInProcess` call
to use `VST3Module::load(bundlePath, /* skipProbe */ true)` instead. This means
`scanOneInProcess` cannot simply delegate to `scanOne()` — it needs its own load call.

Implement `scanOneInProcess` by copying the body of `scanOne()` but replacing the
`VST3Module::load(bundlePath)` call with `VST3Module::load(bundlePath, /* skipProbe */ true)`.
Wrap the entire body with pedal set/clear. If the load succeeds, also update the
probe cache status to `safe`. If it fails, mark as `blocked`.

```cpp
std::optional<PluginDescription> PluginScanner::scanOneInProcess (
    const std::filesystem::path& bundlePath)
{
    if (probeCache_)
        probeCache_->setPedal (bundlePath);

    // Load directly, skip fork probe (yabridge can't be fork-probed)
    auto module = VST3Module::load (bundlePath, /* skipProbe */ true);

    if (probeCache_)
        probeCache_->clearPedal();

    if (! module)
    {
        if (probeCache_)
        {
            probeCache_->setStatus (bundlePath, ProbeCache::Status::blocked);
            probeCache_->save();
        }
        return std::nullopt;
    }

    if (probeCache_)
    {
        probeCache_->setStatus (bundlePath, ProbeCache::Status::safe);
        probeCache_->save();
    }

    // Extract metadata — same logic as scanOne() from the factory query onward
    auto* factory = module->getFactory();
    if (factory == nullptr)
        return std::nullopt;

    // ... (copy the factory iteration logic from scanOne(), lines 215-359)
}
```

To avoid code duplication between `scanOne()` and `scanOneInProcess()`, extract the
factory-querying logic into a private helper:

```cpp
// Add to private in header:
/// Extract PluginDescription from an already-loaded module.
static std::optional<PluginDescription> extractDescription (
    VST3Module& module, const std::filesystem::path& bundlePath);
```

Then refactor `scanOne()` to use it too:
```cpp
std::optional<PluginDescription> PluginScanner::scanOne (
    const std::filesystem::path& bundlePath)
{
    auto module = VST3Module::load (bundlePath);
    if (! module)
        return std::nullopt;
    return extractDescription (*module, bundlePath);
}
```

**Modify `scanAll()` to handle yabridge bundles:**

In the scan loop (around line 522), check `VST3Module::isYabridgeBundle()` before
choosing the scan method:

```cpp
for (int i = 0; i < total; ++i)
{
    const auto& bundle = allBundles[static_cast<size_t>(i)];
    auto pluginName = bundle.stem().string();

    if (progressCallback_)
        progressCallback_ (pluginName, i + 1, total);

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

#### 3. `src/dc/plugins/VST3Host.cpp`

In `VST3Host::scanPlugins()`, pass the probe cache to the scanner before scanning:

```cpp
void VST3Host::scanPlugins (PluginScanner::ProgressCallback cb)
{
    if (cb)
        scanner_.setProgressCallback (std::move (cb));

    scanner_.setProbeCache (&probeCache_);
    knownPlugins_ = scanner_.scanAll();
}
```

### New files

#### 4. `tests/unit/test_yabridge_scan.cpp`

Test the yabridge detection and scan path selection:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "dc/plugins/PluginScanner.h"
#include "dc/plugins/VST3Module.h"

TEST_CASE ("isYabridgeBundle returns false for non-existent path", "[plugins]")
{
    REQUIRE_FALSE (dc::VST3Module::isYabridgeBundle ("/nonexistent/plugin.vst3"));
}

TEST_CASE ("PluginScanner scanOneInProcess does not crash on invalid path", "[plugins]")
{
    dc::PluginScanner scanner;
    auto result = scanner.scanOneInProcess ("/nonexistent/plugin.vst3");
    REQUIRE_FALSE (result.has_value());
}

TEST_CASE ("PluginScanner extractDescription is nullopt for null factory", "[plugins]")
{
    // Indirectly tested via scanOne on a nonexistent path
    dc::PluginScanner scanner;
    auto result = scanner.scanOne ("/nonexistent/plugin.vst3");
    REQUIRE_FALSE (result.has_value());
}
```

Add the new test file to `CMakeLists.txt` under the unit test target's `target_sources`.

## Important: Yabridge Settle Delay

When loading yabridge bundles, `VST3Host::getOrLoadModule()` adds a 500ms settle delay
after each successful load to prevent Wine IPC races. The scanner does NOT need this
delay because the scanner only extracts metadata and immediately releases the module —
it does not keep modules loaded or process audio. The module destructor runs at the end
of `scanOneInProcess()`, cleanly shutting down the Wine bridge.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Header includes use project-relative paths (e.g., `"dc/plugins/PluginScanner.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --build --preset test && ctest --test-dir build-debug --output-on-failure`
