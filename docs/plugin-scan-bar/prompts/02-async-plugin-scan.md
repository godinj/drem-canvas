# Agent: Async Plugin Scan

You are working on the `feature/plugin-scan-bar` branch of Drem Canvas, a C++17 DAW
using Skia for rendering. Your task is to move plugin scanning off the UI thread so it
runs asynchronously, reporting progress via `dc::MessageQueue`.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `src/plugins/PluginManager.h` and `src/plugins/PluginManager.cpp` (current sync scan)
- `src/dc/plugins/PluginScanner.h` (existing `ProgressCallback` signature)
- `src/dc/plugins/VST3Host.h` (accepts `ProgressCallback` in `scanPlugins()`)
- `src/dc/foundation/worker_thread.h` (thread pool for background work)
- `src/dc/foundation/message_queue.h` (post callbacks to message thread)
- `src/ui/AppController.h` (owns `messageQueue` and `pluginManager`)
- `src/ui/AppController.cpp` (search for `messageQueue.processAll()` — pumped in `tick()`)

## Deliverables

### Migration

#### 1. `src/plugins/PluginManager.h`

Add async scan API alongside the existing sync one. Changes:

```cpp
#pragma once
#include "dc/plugins/VST3Host.h"
#include "dc/plugins/PluginDescription.h"
#include "dc/foundation/worker_thread.h"
#include "dc/foundation/message_queue.h"
#include <filesystem>
#include <functional>
#include <vector>

namespace dc
{

class PluginManager
{
public:
    explicit PluginManager (MessageQueue& messageQueue);

    // ─── Existing (keep) ──────────────────────────────────
    void scanForPlugins();  // sync, blocking — keep for backward compat / tests
    const std::vector<dc::PluginDescription>& getKnownPlugins() const;
    dc::VST3Host& getVST3Host();
    void savePluginList (const std::filesystem::path& file) const;
    void loadPluginList (const std::filesystem::path& file);
    std::filesystem::path getDefaultPluginListFile() const;

    // ─── New async API ────────────────────────────────────

    /// Progress callback: (pluginName, current, total)
    using ScanProgressCallback = std::function<void (const std::string& pluginName,
                                                      int current, int total)>;
    /// Called on the message thread when scan finishes
    using ScanCompleteCallback = std::function<void()>;

    /// Start an async scan on a background thread.
    /// `onProgress` is called on the **message thread** for each plugin.
    /// `onComplete` is called on the **message thread** when done.
    /// No-op if a scan is already in progress.
    void scanForPluginsAsync (ScanProgressCallback onProgress = {},
                              ScanCompleteCallback onComplete = {});

    /// True while an async scan is running.
    bool isScanning() const;

private:
    dc::VST3Host vst3Host_;
    dc::MessageQueue& messageQueue_;
    dc::WorkerThread scanThread_ { "plugin-scan" };
    std::atomic<bool> scanning_ { false };

    PluginManager (const PluginManager&) = delete;
    PluginManager& operator= (const PluginManager&) = delete;
};

} // namespace dc
```

#### 2. `src/plugins/PluginManager.cpp`

Implement `scanForPluginsAsync`:

```cpp
void PluginManager::scanForPluginsAsync (ScanProgressCallback onProgress,
                                          ScanCompleteCallback onComplete)
{
    if (scanning_.load())
        return;

    scanning_.store (true);

    scanThread_.submit ([this, onProgress = std::move (onProgress),
                         onComplete = std::move (onComplete)]()
    {
        vst3Host_.scanPlugins ([this, &onProgress] (const std::string& name,
                                                     int current, int total)
        {
            if (onProgress)
            {
                messageQueue_.post ([onProgress, name, current, total]()
                {
                    onProgress (name, current, total);
                });
            }
        });

        savePluginList (getDefaultPluginListFile());

        messageQueue_.post ([this, onComplete]()
        {
            scanning_.store (false);
            if (onComplete)
                onComplete();
        });
    });
}

bool PluginManager::isScanning() const
{
    return scanning_.load();
}
```

Update the constructor to accept `MessageQueue&`:

```cpp
PluginManager::PluginManager (MessageQueue& mq)
    : messageQueue_ (mq)
{
}
```

Keep `scanForPlugins()` unchanged (sync path still useful for tests/CLI).

#### 3. `src/ui/AppController.h` / `src/ui/AppController.cpp`

Update `PluginManager` construction to pass `messageQueue`:

In `AppController.h`, the member declaration order must ensure `messageQueue` is
declared **before** `pluginManager`. Find the existing `PluginManager pluginManager;`
member and change it to:

```cpp
PluginManager pluginManager { messageQueue };
```

If `pluginManager` is declared before `messageQueue` in the member list, reorder
so `messageQueue` comes first (C++ initializes members in declaration order).

No other changes to AppController are needed in this prompt.

### New test (`tests/unit/test_PluginManager_async.cpp`)

Integration test covering:
- `isScanning()` returns false initially
- `scanForPluginsAsync` sets `isScanning()` to true
- After pumping `messageQueue.processAll()` in a loop (with a short sleep),
  `onComplete` fires and `isScanning()` returns false
- Progress callback fires at least once with `current > 0` and `total > 0`
  (skip this assertion if no VST3 bundles are installed — check `total == 0`)

Tag with `[integration]` since it requires filesystem access.

Add the test file to `tests/CMakeLists.txt`.

## Scope Limitation

Do NOT modify `BrowserWidget` or any UI widget in this prompt. The UI wiring is
handled by Agent 03. Only modify `PluginManager` and `AppController` (construction only).

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"dc/plugins/VST3Host.h"`)
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
