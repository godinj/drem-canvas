# Agent: VST3Host + PluginManager/PluginHost Migration

You are working on the `feature/sans-juce-plugin-hosting` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 (VST3 Plugin Hosting): implement `dc::VST3Host` as the top-level
host class, then migrate `src/plugins/PluginManager` and `src/plugins/PluginHost` to use
the new dc:: types instead of JUCE plugin APIs.

## Context

Read these specs before starting:
- `docs/sans-juce/03-plugin-hosting.md` (VST3Host section, State Management, Migration Path steps 7-8)
- `docs/sans-juce/08-migration-guide.md` (Phase 4 — files to migrate: PluginManager, PluginHost)
- `src/plugins/PluginManager.h` (current implementation using `juce::KnownPluginList`, `juce::AudioPluginFormatManager`)
- `src/plugins/PluginManager.cpp` (current implementation)
- `src/plugins/PluginHost.h` (current implementation using `juce::AudioPluginInstance`, `juce::PluginDescription`)
- `src/plugins/PluginHost.cpp` (current implementation)
- `src/dc/plugins/VST3Module.h` (module loading — Agent 01)
- `src/dc/plugins/PluginDescription.h` (metadata — Agent 01)
- `src/dc/plugins/PluginScanner.h` (scanning — Agent 02)
- `src/dc/plugins/PluginInstance.h` (component wrapper — Agent 03)
- `src/dc/foundation/base64.h` (dc::base64Encode, dc::base64Decode)

## Dependencies

This agent depends on:
- Agent 01 (VST3Module, PluginDescription)
- Agent 02 (PluginScanner)
- Agent 03 (PluginInstance, ComponentHandler)

If those files don't exist yet, create stub headers matching the interfaces in
`docs/sans-juce/03-plugin-hosting.md` and implement against them.

## Deliverables

### New files (src/dc/plugins/)

#### 1. VST3Host.h

Top-level host class tying together module loading, scanning, and instantiation.

```cpp
#pragma once
#include "dc/plugins/PluginDescription.h"
#include "dc/plugins/PluginScanner.h"
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/VST3Module.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

class VST3Host
{
public:
    VST3Host();
    ~VST3Host();

    /// Scan for plugins (out-of-process, crash-safe)
    void scanPlugins(PluginScanner::ProgressCallback cb = {});

    /// Get the known plugin database
    const std::vector<PluginDescription>& getKnownPlugins() const;

    /// Set the known plugin list (e.g., after loading from YAML)
    void setKnownPlugins(std::vector<PluginDescription> plugins);

    /// Load plugin database from a YAML file
    void loadDatabase(const std::filesystem::path& path);

    /// Save plugin database to a YAML file
    void saveDatabase(const std::filesystem::path& path) const;

    /// Create a plugin instance (sync, blocking)
    std::unique_ptr<PluginInstance> createInstanceSync(
        const PluginDescription& desc,
        double sampleRate, int maxBlockSize);

    /// Create a plugin instance (async, callback on completion)
    using CreateCallback = std::function<void(
        std::unique_ptr<PluginInstance> instance,
        std::string error)>;
    void createInstance(const PluginDescription& desc,
                        double sampleRate, int maxBlockSize,
                        CreateCallback callback);

    /// Find a plugin description by UID
    const PluginDescription* findByUid(const std::string& uid) const;

private:
    PluginScanner scanner_;
    std::vector<PluginDescription> knownPlugins_;

    /// Cache of loaded modules (keyed by bundle path string)
    std::unordered_map<std::string, std::unique_ptr<VST3Module>> loadedModules_;

    /// Get or load a module for a bundle path
    VST3Module* getOrLoadModule(const std::filesystem::path& bundlePath);
};

} // namespace dc
```

#### 2. VST3Host.cpp

Implementation details:

**`scanPlugins(cb)`:**
1. Set the progress callback on `scanner_`
2. Call `scanner_.scanAll()`
3. Store results in `knownPlugins_`

**`loadDatabase(path)` / `saveDatabase(path)`:**
- Use yaml-cpp to read/write a YAML file containing an array of plugin descriptions
- Each plugin is serialized via `PluginDescription::toMap()`
- Format:
  ```yaml
  plugins:
    - name: "MyPlugin"
      manufacturer: "Acme"
      uid: "0123456789ABCDEF0123456789ABCDEF"
      path: "/usr/lib/vst3/MyPlugin.vst3"
      ...
  ```
- Include `<yaml-cpp/yaml.h>`

**`createInstanceSync(desc, sampleRate, maxBlockSize)`:**
1. Get or load the module: `getOrLoadModule(desc.path)`
2. If module fails to load, return nullptr
3. Call `PluginInstance::create(*module, desc, sampleRate, maxBlockSize)`
4. Return the instance

**`createInstance(desc, sampleRate, maxBlockSize, callback)` (async):**
- For the initial implementation, run synchronously on a background thread using
  `dc::WorkerThread` (from `dc/foundation/worker_thread.h`) or `std::thread`:
  1. Spawn a thread
  2. Call `createInstanceSync()` in the thread
  3. Post the result back to the message thread via `dc::MessageQueue`
  4. Call `callback(instance, error)` on the message thread
- If you use `std::thread`, detach it (the callback handles the result)
- Error string: empty on success, description of failure on error

**`getOrLoadModule(bundlePath)`:**
- Check `loadedModules_` cache by `bundlePath.string()`
- If not found, call `VST3Module::load(bundlePath)` and insert into cache
- Return raw pointer (module lifetime managed by the cache)

**`findByUid(uid)`:**
- Linear search through `knownPlugins_` for matching `uid`
- Return pointer to the description, or nullptr

### Migration

#### 3. src/plugins/PluginManager.h/.cpp

Replace JUCE plugin management with `dc::VST3Host`.

**Before (JUCE types to remove):**
```cpp
#include <JuceHeader.h>
juce::KnownPluginList knownPlugins;
juce::AudioPluginFormatManager formatManager;
```

**After:**
```cpp
#include "dc/plugins/VST3Host.h"
dc::VST3Host vst3Host_;
```

**Method migrations:**

| Old method | New method |
|-----------|-----------|
| `scanForPlugins()` | `vst3Host_.scanPlugins()` |
| `scanDefaultPaths()` | `vst3Host_.scanPlugins()` (default paths built-in) |
| `getKnownPlugins()` returning `juce::KnownPluginList&` | `getKnownPlugins()` returning `const std::vector<dc::PluginDescription>&` via `vst3Host_.getKnownPlugins()` |
| `getFormatManager()` returning `juce::AudioPluginFormatManager&` | Remove entirely — no longer needed |
| `savePluginList(path)` using XML | `vst3Host_.saveDatabase(path)` using YAML |
| `loadPluginList(path)` using XML | `vst3Host_.loadDatabase(path)` using YAML |
| `getDefaultPluginListFile()` | Keep, but change extension from `.xml` to `.yaml` |

**Remove these includes:**
- `<JuceHeader.h>`

**Add these includes:**
- `"dc/plugins/VST3Host.h"`
- `"dc/plugins/PluginDescription.h"`

**Public interface changes:**
```cpp
// Old:
const juce::KnownPluginList& getKnownPlugins() const;
juce::KnownPluginList& getKnownPlugins();
juce::AudioPluginFormatManager& getFormatManager();

// New:
const std::vector<dc::PluginDescription>& getKnownPlugins() const;
dc::VST3Host& getVST3Host();
```

**Callers of PluginManager**: Search the codebase for all call sites of `getKnownPlugins()`
and `getFormatManager()` and update them. Common callers include:
- `PluginHost` (migrated below)
- UI code listing available plugins (update to iterate `vector<PluginDescription>`)

#### 4. src/plugins/PluginHost.h/.cpp

Replace JUCE plugin instantiation with `dc::VST3Host` / `dc::PluginInstance`.

**Before (JUCE types to remove):**
```cpp
#include <JuceHeader.h>
using PluginCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance>, const std::string&)>;
void createPluginAsync(const juce::PluginDescription& desc, ...);
static std::unique_ptr<juce::AudioPluginInstance> createPluginSync(...);
static std::string savePluginState(juce::AudioPluginInstance& plugin);
static void restorePluginState(juce::AudioPluginInstance& plugin, const std::string& base64State);
static juce::PluginDescription descriptionFromPropertyTree(const PropertyTree& pluginNode);
```

**After:**
```cpp
#include "dc/plugins/VST3Host.h"
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginDescription.h"
#include "dc/foundation/base64.h"

using PluginCallback = std::function<void(std::unique_ptr<dc::PluginInstance>, const std::string&)>;
void createPluginAsync(const dc::PluginDescription& desc,
                       double sampleRate, int blockSize,
                       PluginCallback callback);
std::unique_ptr<dc::PluginInstance> createPluginSync(
    const dc::PluginDescription& desc,
    double sampleRate, int blockSize);
static std::string savePluginState(dc::PluginInstance& plugin);
static void restorePluginState(dc::PluginInstance& plugin, const std::string& base64State);
static dc::PluginDescription descriptionFromPropertyTree(const PropertyTree& pluginNode);
```

**State save/restore migration:**

```cpp
// Old (JUCE):
juce::MemoryBlock stateData;
plugin.getStateInformation(stateData);
return stateData.toBase64Encoding().toStdString();

// New (dc::):
auto state = plugin.getState();  // returns vector<uint8_t>
return dc::base64Encode(state);

// Old restore:
juce::MemoryBlock restoreData;
restoreData.fromBase64Encoding(juce::String(base64State));
plugin.setStateInformation(restoreData.getData(), restoreData.getSize());

// New restore:
auto state = dc::base64Decode(base64State);
plugin.setState(state);
```

**`descriptionFromPropertyTree` migration:**
- Read properties from `PropertyTree` nodes and construct `dc::PluginDescription`
  instead of `juce::PluginDescription`
- Map property names: `pluginName` → `name`, `pluginManufacturer` → `manufacturer`,
  `pluginUID` → `uid`, `pluginPath` → `path`, etc.

**Remove these includes:**
- `<JuceHeader.h>`

**Internal reference changes:**
- The constructor takes `PluginManager&` — change internal calls from
  `manager_.getFormatManager()` to `manager_.getVST3Host()`
- `createPluginAsync` delegates to `vst3Host.createInstance()`
- `createPluginSync` delegates to `vst3Host.createInstanceSync()`

### Modified files

#### 5. CMakeLists.txt

Add `src/dc/plugins/VST3Host.cpp` to `target_sources` under `# dc::plugins library`.

## Scope Limitation

This agent modifies ONLY `PluginManager.h/.cpp`, `PluginHost.h/.cpp`, and creates
`VST3Host.h/.cpp`. Do NOT modify platform bridges, PluginWindowManager,
ParameterFinderScanner, or TrackProcessor — those are handled by Agent 06.

When callers of `PluginManager` or `PluginHost` use types that are changing
(e.g., `juce::AudioPluginInstance` → `dc::PluginInstance`), update the call sites
that are in `PluginManager.cpp` and `PluginHost.cpp`. For callers in OTHER files
(like `TrackProcessor`, `PluginWindowManager`), leave `// TODO: Phase 4 Agent 06`
comments if the types don't match yet — those files will be migrated by Agent 06.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Use `dc_log()` from `dc/foundation/assert.h` for logging
- Use yaml-cpp for database persistence (already a project dependency)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
