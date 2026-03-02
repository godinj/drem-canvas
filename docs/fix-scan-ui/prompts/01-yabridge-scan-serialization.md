# Agent: Yabridge Scan Serialization

You are working on the `feature/fix-scan-ui` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to fix a bug where all yabridge plugins get marked as `blocked` in the ProbeCache after scanning, causing them to disappear from the plugin list on subsequent launches.

## Context

Read these files before starting:
- `src/dc/plugins/PluginScanner.h` (class definition — note missing mutex)
- `src/dc/plugins/PluginScanner.cpp` (`scanOneInProcess()` at line 383 — no settle delay; `scanAll()` at line 546 — calls `scanOneInProcess()` in tight loop)
- `src/dc/plugins/VST3Host.cpp` (`getOrLoadModule()` at line 151 — reference implementation with `yabridgeLoadMutex_` and 500ms settle delay)
- `src/dc/plugins/VST3Host.h` (line 73-79 — `yabridgeLoadMutex_` declaration and comment)
- `src/dc/plugins/VST3Module.h` (`isYabridgeBundle()` static method)
- `src/dc/plugins/ProbeCache.h` (ProbeCache API)

## Root Cause

`PluginScanner::scanOneInProcess()` loads yabridge bundles directly via `VST3Module::load()`. Yabridge chainloaders spawn Wine host processes whose IPC setup races when two loads overlap, producing a SIGSEGV on a yabridge bridge thread. The scanner calls `scanOneInProcess()` in a tight loop from `scanAll()` with no serialization between loads.

Compare with `VST3Host::getOrLoadModule()` which correctly:
1. Holds `yabridgeLoadMutex_` for the full load cycle
2. Sleeps 500ms after a successful yabridge load to let the Wine bridge settle

The scanner has neither of these protections, so rapid successive loads crash and every yabridge plugin gets marked `blocked`.

## Deliverables

### Modified files

#### 1. `src/dc/plugins/PluginScanner.h`

Add a `std::mutex` member for yabridge load serialization:

- Add `#include <mutex>` to includes
- Add private member: `std::mutex yabridgeLoadMutex_;`

#### 2. `src/dc/plugins/PluginScanner.cpp`

Modify `scanOneInProcess()` to serialize yabridge loads with a settle delay:

- Add `#include <thread>` and `#include <chrono>` to includes (for `std::this_thread::sleep_for`)
- At the top of `scanOneInProcess()`, check `VST3Module::isYabridgeBundle(bundlePath)` and if true, lock `yabridgeLoadMutex_` using `std::unique_lock`
- After a successful load (after `extractDescription` but before return), if yabridge, sleep 500ms to let the Wine bridge settle — matching the pattern in `VST3Host::getOrLoadModule()`
- The lock must be held for the entire load+settle cycle

The modified `scanOneInProcess()` should look like:

```cpp
std::optional<PluginDescription> PluginScanner::scanOneInProcess (
    const std::filesystem::path& bundlePath)
{
    bool isYabridge = VST3Module::isYabridgeBundle (bundlePath);

    // Serialize yabridge loads — Wine bridge IPC setup races if two
    // loads overlap, producing SIGSEGV on bridge threads.
    std::unique_lock<std::mutex> lock (yabridgeLoadMutex_, std::defer_lock);
    if (isYabridge)
        lock.lock();

    if (probeCache_)
        probeCache_->setPedal (bundlePath);

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

    auto desc = extractDescription (*module, bundlePath);

    // Let the Wine bridge settle before loading the next yabridge plugin.
    if (isYabridge)
        std::this_thread::sleep_for (std::chrono::milliseconds (500));

    return desc;
}
```

## Scope Limitation

- Only modify `PluginScanner.h` and `PluginScanner.cpp`
- Do not modify `VST3Host`, `ProbeCache`, `VST3Module`, `PluginManager`, or any UI code
- Do not change the `scanAll()` loop structure or the `scanOneForked()` method
- Do not add new files

## Conventions

- Namespace: `dc`
- Spaces around operators, braces on new line for classes/functions
- `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
