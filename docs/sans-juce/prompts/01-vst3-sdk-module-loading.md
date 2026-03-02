# Agent: VST3 SDK Integration + Module Loading + PluginDescription

You are working on the `feature/sans-juce-plugin-hosting` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 (VST3 Plugin Hosting): add the Steinberg VST3 SDK as a dependency,
implement `dc::VST3Module` for loading VST3 bundles via `dlopen`, and implement
`dc::PluginDescription` as a metadata struct.

## Context

Read these specs before starting:
- `docs/sans-juce/03-plugin-hosting.md` (Module Loading, PluginDescription sections)
- `docs/sans-juce/08-migration-guide.md` (Phase 4 section — new files to create)
- `CMakeLists.txt` (lines 12-25 for FetchContent pattern, lines 119-180 for source listing, lines 388-431 for link targets)

## Prerequisites

Add the Steinberg VST3 SDK to the project. Use `FetchContent` to download it (similar
to how yaml-cpp is fetched in `CMakeLists.txt` lines 17-25). Pin to a stable release tag
(v3.7.12 or latest stable).

```cmake
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.7.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vst3sdk)
```

Link the VST3 SDK to the target:

```cmake
target_link_libraries(DremCanvas PRIVATE sdk)
```

Add the VST3 SDK include path so `pluginterfaces/`, `public.sdk/`, etc. are available:

```cmake
target_include_directories(DremCanvas PRIVATE ${vst3sdk_SOURCE_DIR})
```

**Important**: The VST3 SDK's CMake may define many targets. Only link `sdk` (the hosting
library). Do NOT link `sdk_hosting` if it pulls in unnecessary GUI code. Test that the
build succeeds with just `sdk`. If `sdk` alone is insufficient, try `sdk_hosting` as
a fallback.

## Deliverables

### New files (src/dc/plugins/)

#### 1. VST3Module.h

Header for VST3 module loading.

```cpp
#pragma once
#include <filesystem>
#include <memory>

namespace Steinberg { class IPluginFactory; }

namespace dc {

class VST3Module
{
public:
    /// Load a VST3 module from a bundle path.
    /// Returns nullptr on failure (logs the error).
    static std::unique_ptr<VST3Module> load(
        const std::filesystem::path& bundlePath);

    ~VST3Module();

    VST3Module(const VST3Module&) = delete;
    VST3Module& operator=(const VST3Module&) = delete;

    /// Get the plugin factory (never null after successful load)
    Steinberg::IPluginFactory* getFactory() const;

    /// Get the bundle path
    const std::filesystem::path& getPath() const;

private:
    VST3Module() = default;

    void* libraryHandle_ = nullptr;  // dlopen handle
    Steinberg::IPluginFactory* factory_ = nullptr;
    std::filesystem::path path_;

    using InitModuleFunc = bool (*)();
    using ExitModuleFunc = bool (*)();
};

} // namespace dc
```

#### 2. VST3Module.cpp

Implementation of VST3 module loading.

- **Bundle path resolution**: A `.vst3` bundle contains the shared library at:
  - Linux: `<bundle>/Contents/x86_64-linux/<name>.so`
  - macOS: `<bundle>/Contents/MacOS/<name>`
  - Use `std::filesystem` to construct the platform-appropriate path
  - The `<name>` is the bundle stem (e.g., `MyPlugin.vst3` → `MyPlugin`)
- **Loading sequence**:
  1. `dlopen()` the shared library (Linux) or `dlopen()` (macOS)
  2. Look up `InitDll` / `ExitDll` (Linux) or `bundleEntry` / `bundleExit` (macOS) via `dlsym()`
  3. Call the init function
  4. Look up `GetPluginFactory` via `dlsym()`
  5. Call `GetPluginFactory()` to obtain `IPluginFactory*`
- **Error handling**: Log errors with `dc_log()` (from `dc/foundation/assert.h`), return nullptr
- **Destructor**: Call exit function, then `dlclose()`
- **Include**: `<dlfcn.h>` for `dlopen`/`dlsym`/`dlclose`

#### 3. PluginDescription.h

Metadata struct for discovered plugins.

```cpp
#pragma once
#include <filesystem>
#include <map>
#include <string>

namespace dc {

struct PluginDescription
{
    std::string name;
    std::string manufacturer;
    std::string category;
    std::string version;
    std::string uid;              // VST3 class UID as hex string (32 chars)
    std::filesystem::path path;   // .vst3 bundle path
    int numInputChannels = 0;
    int numOutputChannels = 0;
    bool hasEditor = false;
    bool acceptsMidi = false;
    bool producesMidi = false;

    /// Serialize to a string map (for YAML persistence)
    std::map<std::string, std::string> toMap() const;

    /// Deserialize from a string map
    static PluginDescription fromMap(const std::map<std::string, std::string>& m);

    /// Convert a Steinberg FUID to a 32-char hex string
    static std::string uidToHexString(const char uid[16]);

    /// Convert a 32-char hex string back to a 16-byte UID
    static bool hexStringToUid(const std::string& hex, char uid[16]);
};

} // namespace dc
```

- `toMap()` / `fromMap()`: Convert all fields to/from `std::map<std::string, std::string>`.
  Integer and bool fields use `std::to_string()` / `std::stoi()`. Path uses `.string()`.
- `uidToHexString()`: Convert 16 raw bytes to 32 hex characters (lowercase).
- `hexStringToUid()`: Parse 32 hex chars back to 16 bytes. Return false on invalid input.

### Modified files

#### 4. CMakeLists.txt

- Add `FetchContent_Declare` + `FetchContent_MakeAvailable` for VST3 SDK (near the
  yaml-cpp FetchContent block, around line 17-25)
- Add new source files to `target_sources` under a `# dc::plugins library` comment
  (after the `# dc::audio library` section, around line 142):
  ```cmake
  # dc::plugins library
  src/dc/plugins/VST3Module.cpp
  ```
  (PluginDescription.h is header-only — implement `toMap`/`fromMap`/`uidToHexString`/
  `hexStringToUid` inline or add a `.cpp` if preferred)
- Add `target_link_libraries(DremCanvas PRIVATE ... sdk)` to the link section
- Add VST3 SDK include directory to `target_include_directories`

## Scope Limitation

This agent ONLY creates the VST3 SDK integration, module loading, and description struct.
Do NOT modify any existing `src/plugins/` files. Do NOT implement scanning, instantiation,
or editor hosting — those are separate agents.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Use `dc_log()` from `dc/foundation/assert.h` for logging
- Build verification: `cmake --build --preset release`
