# Agent: PluginScanner — Out-of-Process VST3 Scanning

You are working on the `feature/sans-juce-plugin-hosting` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 (VST3 Plugin Hosting): implement `dc::PluginScanner` for discovering
VST3 plugins with crash isolation via forked subprocesses.

## Context

Read these specs before starting:
- `docs/sans-juce/03-plugin-hosting.md` (PluginScanner section, Default Search Paths, Out-of-Process Scanning)
- `docs/sans-juce/08-migration-guide.md` (Phase 4 — new files to create)
- `src/dc/plugins/VST3Module.h` (module loading interface — created by Agent 01)
- `src/dc/plugins/PluginDescription.h` (metadata struct — created by Agent 01)

## Dependencies

This agent depends on Agent 01 (VST3 SDK + Module Loading + PluginDescription).
If `VST3Module.h` and `PluginDescription.h` don't exist yet, create stub headers
matching the interfaces from `docs/sans-juce/03-plugin-hosting.md` and implement
against them.

## Deliverables

### New files (src/dc/plugins/)

#### 1. PluginScanner.h

```cpp
#pragma once
#include "PluginDescription.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dc {

class PluginScanner
{
public:
    PluginScanner();

    /// Scan all standard VST3 directories. Returns all discovered plugins.
    /// Each plugin is scanned in a forked subprocess for crash isolation.
    std::vector<PluginDescription> scanAll();

    /// Scan a single .vst3 bundle path (in-process, no fork).
    /// Returns nullopt if the bundle is invalid or crashes are caught.
    std::optional<PluginDescription> scanOne(
        const std::filesystem::path& bundlePath);

    /// Get standard VST3 search paths for the current platform.
    static std::vector<std::filesystem::path> getDefaultSearchPaths();

    /// Progress callback: (pluginName, current, total)
    using ProgressCallback = std::function<void(const std::string& pluginName,
                                                 int current, int total)>;
    void setProgressCallback(ProgressCallback cb);

private:
    ProgressCallback progressCallback_;
    std::filesystem::path deadMansPedal_;  // tracks current scan target

    /// Scan a single bundle in a forked child process.
    /// Returns nullopt if child crashes or times out.
    std::optional<PluginDescription> scanOneForked(
        const std::filesystem::path& bundlePath);

    /// Enumerate .vst3 bundles in a directory (non-recursive into bundles).
    static std::vector<std::filesystem::path> findBundles(
        const std::filesystem::path& searchDir);
};

} // namespace dc
```

#### 2. PluginScanner.cpp

Implementation of out-of-process scanning.

**`getDefaultSearchPaths()`:**
- Linux: `~/.vst3/`, `/usr/lib/vst3/`, `/usr/local/lib/vst3/`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`, `/Library/Audio/Plug-Ins/VST3/`
- Use `std::filesystem::path` and expand `~` via `getenv("HOME")`
- Only return paths that exist (`std::filesystem::is_directory`)

**`findBundles(searchDir)`:**
- Iterate `searchDir` with `std::filesystem::directory_iterator`
- Collect entries with `.vst3` extension (case-insensitive)
- Return sorted vector of paths

**`scanOne(bundlePath)` (in-process):**
1. Load the module: `VST3Module::load(bundlePath)`
2. If load fails, return `nullopt`
3. Get the factory: `module->getFactory()`
4. Query `IPluginFactory::countClasses()` and iterate with `getClassInfo()`
5. For each class with `kVstAudioEffectClass` category:
   - Extract name, manufacturer (from `IPluginFactory2` if available), category, version
   - Convert the class UID to hex string via `PluginDescription::uidToHexString()`
   - Try to create the component (`factory->createInstance()`) to query:
     - `IComponent::getBusCount()` for input/output channel counts
     - `IEditController` availability (check `queryInterface` or `getControllerClassId`)
     - MIDI support: check for event input/output bus types
   - Release the component after querying
6. Return the first valid `PluginDescription` found
   (most bundles contain exactly one audio effect)

**`scanOneForked(bundlePath)` (out-of-process):**
1. Write `bundlePath` to `deadMansPedal_` file (for crash recovery)
2. Create a pipe: `int pipefd[2]; pipe(pipefd);`
3. `fork()`:
   - **Child process**:
     - Close read end of pipe
     - Call `scanOne(bundlePath)` (in-process within the child)
     - If successful, serialize `PluginDescription` via `toMap()`, write to pipe
       as simple key=value lines terminated by a blank line
     - `_exit(0)` on success, `_exit(1)` on failure
   - **Parent process**:
     - Close write end of pipe
     - `waitpid()` with a timeout (use `alarm()` or a polling loop, 10 second timeout)
     - If child exits normally with status 0, read pipe and `fromMap()` the result
     - If child crashes or times out, log the failure, return `nullopt`
4. Delete `deadMansPedal_` on completion

**`scanAll()`:**
1. Collect all `.vst3` bundles from `getDefaultSearchPaths()` via `findBundles()`
2. For each bundle, call `scanOneForked(bundlePath)`
3. Report progress via `progressCallback_` if set
4. Collect successful results into a `vector<PluginDescription>`
5. Return the vector

**Serialization over pipe**: Use the simple format:
```
name=MyPlugin
manufacturer=Acme
category=Fx
version=1.0.0
uid=0123456789ABCDEF0123456789ABCDEF
path=/usr/lib/vst3/MyPlugin.vst3
numInputChannels=2
numOutputChannels=2
hasEditor=1
acceptsMidi=0
producesMidi=0

```
(blank line terminates the record)

**Include headers**: `<unistd.h>` (fork, pipe, read, write), `<sys/wait.h>` (waitpid),
`<signal.h>` (kill for timeout), `<cstring>`, `<sstream>`

### Modified files

#### 3. CMakeLists.txt

Add `src/dc/plugins/PluginScanner.cpp` to `target_sources` under the `# dc::plugins library`
comment (created by Agent 01).

## Scope Limitation

This agent ONLY implements the scanner. Do NOT modify existing `src/plugins/PluginManager.h/.cpp` —
that migration is handled by Agent 05.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Use `dc_log()` from `dc/foundation/assert.h` for logging
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
