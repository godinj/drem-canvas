# 03 — VST3 Plugin Hosting from Steinberg SDK

> Replaces JUCE's plugin hosting layer (`AudioPluginFormatManager`,
> `AudioPluginInstance`, `KnownPluginList`, `AudioProcessorEditor`) with
> direct Steinberg VST3 SDK integration. VST3 only — no AU, no CLAP.

**Phase**: 4 (VST3 Plugin Hosting)
**Dependencies**: Phase 3 (AudioNode interface)
**Related**: [02-audio-graph.md](02-audio-graph.md), [04-foundation-types.md](04-foundation-types.md), [08-migration-guide.md](08-migration-guide.md)

---

## Overview

Drem Canvas currently uses JUCE's plugin hosting APIs:

| JUCE class | Purpose |
|------------|---------|
| `AudioPluginFormatManager` | Format registry (VST3, AU) |
| `KnownPluginList` | Scanned plugin database |
| `PluginDirectoryScanner` | Scan directories for plugins |
| `PluginDescription` | Plugin metadata |
| `AudioPluginInstance` | Loaded plugin (processes audio) |
| `AudioProcessorEditor` | Plugin GUI |
| `AudioProcessorParameter` | Parameter interface |
| `MemoryBlock` | State save/restore blobs |

JUCE wraps the Steinberg VST3 SDK internally. Going direct eliminates:
- The JUCE abstraction layer and its quirks
- The need for JUCE patches (IParameterFinder, performEdit snoop)
- AU support (simplifies to VST3-only)

### JUCE Patches Eliminated

The local JUCE patches (`scripts/juce-patches/`) add:

1. **IParameterFinder spatial hints** — Allows querying which VST3 parameter
   is at a given (x,y) coordinate in the plugin editor. Used for vimium-style
   parameter overlay.

2. **performEdit snoop** — Intercepts `IComponentHandler::performEdit()` calls
   from the plugin to detect which parameter changed during wiggle/probe
   operations.

Both of these become native features when hosting directly — they're just
standard VST3 SDK interfaces (`IParameterFinder` is a Steinberg interface,
and `IComponentHandler` is our implementation).

---

## Current JUCE Plugin Usage

### PluginManager (src/plugins/PluginManager.h)

```cpp
juce::AudioPluginFormatManager formatManager;
juce::KnownPluginList knownPlugins;

// Scanning
formatManager.addDefaultFormats();
auto scanner = std::make_unique<juce::PluginDirectoryScanner>(
    knownPlugins, *formatManager.getFormat(0),
    formatManager.getDefaultLocationsToSearch(),
    true, deadMansPedal);

// Plugin database persistence
auto xml = knownPlugins.createXml();
knownPlugins.recreateFromXml(*xml);
```

### PluginHost (src/plugins/PluginHost.h)

```cpp
// Async instantiation
formatManager.createPluginInstanceAsync(description, sampleRate, blockSize,
    [](auto instance, auto error) { ... });

// State management
juce::MemoryBlock stateData;
plugin->getStateInformation(stateData);
auto base64 = stateData.toBase64Encoding();

juce::MemoryBlock restoreData;
restoreData.fromBase64Encoding(base64String);
plugin->setStateInformation(restoreData.getData(), restoreData.getSize());
```

### Plugin Parameters

```cpp
auto& params = plugin->getParameters();
for (auto* param : params)
{
    param->getName(128);
    param->getValue();
    param->setValue(newValue);
    param->getParameterID();
}
```

### Plugin Editor (platform-specific bridges)

```cpp
auto* editor = plugin->createEditor();
editor->setSize(width, height);
// Embed in native window (Mac: NSView, Linux: X11 reparenting)
```

---

## Design: Steinberg VST3 SDK Integration

### SDK Interfaces Used

| Interface | Purpose |
|-----------|---------|
| `IPluginFactory` | Enumerate components in a module |
| `IComponent` | Audio processor component |
| `IAudioProcessor` | Audio processing (setup, process) |
| `IEditController` | Parameter management, UI |
| `IComponentHandler` | Our implementation — receives parameter edits |
| `IPlugView` | Plugin editor window |
| `IParameterFinder` | Spatial parameter lookup (x,y → paramId) |
| `IPlugFrame` | Our implementation — plugin requests resize |
| `IConnectionPoint` | Component ↔ Controller connection |

### Module Loading

VST3 plugins are packaged as bundles (`.vst3`). Each bundle contains a
shared library exporting `GetPluginFactory()`.

```cpp
namespace dc {

class VST3Module
{
public:
    /// Load a VST3 module from a bundle path
    static std::unique_ptr<VST3Module> load(
        const std::filesystem::path& bundlePath);

    ~VST3Module();

    /// Get the plugin factory
    Steinberg::IPluginFactory* getFactory() const;

    /// Get the bundle path
    const std::filesystem::path& getPath() const;

private:
    void* libraryHandle_ = nullptr;  // dlopen handle
    Steinberg::IPluginFactory* factory_ = nullptr;
    std::filesystem::path path_;
};

} // namespace dc
```

**Platform-specific loading**:
- Linux: `dlopen()` on `Contents/x86_64-linux/MODULE.so`
- macOS: `dlopen()` on `Contents/MacOS/MODULE`

---

## Design: `dc::PluginDescription`

Metadata struct for discovered plugins. Stored in the plugin database.

```cpp
namespace dc {

struct PluginDescription
{
    std::string name;
    std::string manufacturer;
    std::string category;
    std::string version;
    std::string uid;              // VST3 class UID as hex string
    std::filesystem::path path;   // bundle path
    int numInputChannels = 0;
    int numOutputChannels = 0;
    bool hasEditor = false;
    bool acceptsMidi = false;
    bool producesMidi = false;

    /// Serialize to YAML-compatible map
    std::map<std::string, std::string> toMap() const;
    static PluginDescription fromMap(const std::map<std::string, std::string>& m);
};

} // namespace dc
```

---

## Design: `dc::PluginScanner`

Out-of-process plugin scanner with crash isolation.

```cpp
namespace dc {

class PluginScanner
{
public:
    PluginScanner();

    /// Scan standard VST3 directories for plugins
    /// Returns discovered plugins. Crash-safe: each plugin scanned in
    /// a subprocess; crashes don't bring down the host.
    std::vector<PluginDescription> scanAll();

    /// Scan a single bundle path
    std::optional<PluginDescription> scanOne(
        const std::filesystem::path& bundlePath);

    /// Get standard VST3 search paths for the current platform
    static std::vector<std::filesystem::path> getDefaultSearchPaths();

    /// Callback for progress reporting
    using ProgressCallback = std::function<void(const std::string& pluginName,
                                                 int current, int total)>;
    void setProgressCallback(ProgressCallback cb);

private:
    ProgressCallback progressCallback_;
    std::filesystem::path deadMansPedal_;  // tracks current scan target
};

} // namespace dc
```

### Default Search Paths

```
Linux:
  ~/.vst3/
  /usr/lib/vst3/
  /usr/local/lib/vst3/

macOS:
  ~/Library/Audio/Plug-Ins/VST3/
  /Library/Audio/Plug-Ins/VST3/
```

### Out-of-Process Scanning

Each plugin is scanned in a forked subprocess:

1. Fork a child process
2. Child loads the VST3 module, queries the factory, extracts metadata
3. Child writes `PluginDescription` to a pipe (serialized as JSON or binary)
4. Parent reads the description
5. If the child crashes (SIGSEGV, SIGABRT), parent logs the failure and continues

This prevents a buggy plugin from crashing the entire scan.

---

## Design: `dc::PluginInstance`

Wraps a loaded VST3 component. Implements `dc::AudioNode` for graph integration.

```cpp
namespace dc {

class PluginInstance : public AudioNode
{
public:
    /// Create from a loaded module and class info
    static std::unique_ptr<PluginInstance> create(
        VST3Module& module,
        const PluginDescription& desc,
        double sampleRate,
        int maxBlockSize);

    ~PluginInstance();

    // --- AudioNode interface ---
    void prepare(double sampleRate, int maxBlockSize) override;
    void release() override;
    void process(AudioBlock& audio, MidiBlock& midi, int numSamples) override;
    int getLatencySamples() const override;
    int getNumInputChannels() const override;
    int getNumOutputChannels() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    std::string getName() const override;

    // --- Parameters ---
    int getNumParameters() const;
    std::string getParameterName(int index) const;
    std::string getParameterLabel(int index) const;
    float getParameterValue(int index) const;       // 0.0-1.0 normalized
    void setParameterValue(int index, float value);
    Steinberg::Vst::ParamID getParameterId(int index) const;
    std::string getParameterDisplay(int index) const;

    // --- State ---
    std::vector<uint8_t> getState() const;
    void setState(const std::vector<uint8_t>& data);

    // --- Editor ---
    bool hasEditor() const;
    std::unique_ptr<PluginEditor> createEditor();

    // --- IParameterFinder (spatial hints) ---
    bool supportsParameterFinder() const;
    /// Find parameter at screen coordinates (relative to editor view)
    /// Returns parameter index, or -1 if none found
    int findParameterAtPoint(int x, int y) const;

    // --- performEdit snoop ---
    /// Get the last parameter that was edited by the plugin UI
    /// (via IComponentHandler::performEdit)
    struct EditEvent { Steinberg::Vst::ParamID paramId; double value; };
    std::optional<EditEvent> popLastEdit();

    // --- Description ---
    const PluginDescription& getDescription() const;

private:
    class ComponentHandler;  // implements IComponentHandler

    Steinberg::Vst::IComponent* component_ = nullptr;
    Steinberg::Vst::IAudioProcessor* processor_ = nullptr;
    Steinberg::Vst::IEditController* controller_ = nullptr;
    Steinberg::Vst::IParameterFinder* parameterFinder_ = nullptr;
    std::unique_ptr<ComponentHandler> handler_;
    PluginDescription description_;

    // Audio processing buffers
    Steinberg::Vst::ProcessData processData_;
    Steinberg::Vst::AudioBusBuffers inputBusBuffers_;
    Steinberg::Vst::AudioBusBuffers outputBusBuffers_;
    std::vector<Steinberg::Vst::Event> eventList_;

    // Parameter cache
    struct ParamInfo
    {
        Steinberg::Vst::ParamID id;
        std::string name;
        std::string label;
    };
    std::vector<ParamInfo> parameters_;

    // performEdit snoop queue
    dc::SPSCQueue<EditEvent> editEvents_{64};
};

} // namespace dc
```

### IComponentHandler Implementation

This is our host-side implementation of `Steinberg::Vst::IComponentHandler`.
The plugin calls methods on this when the user interacts with the plugin UI.

```cpp
class PluginInstance::ComponentHandler
    : public Steinberg::Vst::IComponentHandler
{
public:
    // Called when plugin begins editing a parameter (mouse down on knob)
    tresult beginEdit(Steinberg::Vst::ParamID id) override;

    // Called when plugin changes a parameter value (mouse drag)
    tresult performEdit(Steinberg::Vst::ParamID id,
                        Steinberg::Vst::ParamValue valueNormalized) override;

    // Called when plugin finishes editing (mouse up)
    tresult endEdit(Steinberg::Vst::ParamID id) override;

    // Called when plugin requests a restart (latency change, etc.)
    tresult restartComponent(int32 flags) override;

    // --- Our extensions ---

    /// Queue for snooping performEdit calls (for parameter finder)
    dc::SPSCQueue<EditEvent>& getEditQueue();

    /// Notify PropertyTree of parameter changes (for automation recording)
    void setParameterChangeCallback(
        std::function<void(Steinberg::Vst::ParamID, double)> cb);

private:
    dc::SPSCQueue<EditEvent>& editQueue_;
    std::function<void(Steinberg::Vst::ParamID, double)> paramCallback_;
};
```

**performEdit snoop**: When the plugin calls `performEdit()`, the handler
pushes the event to the SPSC queue. The `ParameterFinderScanner` polls this
queue during wiggle operations to detect which parameter changed. This
replaces the JUCE patch that intercepted the same call.

---

## Design: `dc::PluginEditor`

Manages the VST3 `IPlugView` lifecycle and embeds the plugin UI in a
platform window.

```cpp
namespace dc {

class PluginEditor
{
public:
    PluginEditor(Steinberg::IPlugView* view, PluginInstance& instance);
    ~PluginEditor();

    /// Get the editor's preferred size
    std::pair<int, int> getPreferredSize() const;

    /// Attach to a native window handle
    /// macOS: NSView*
    /// Linux: X11 Window (XID)
    void attachToWindow(void* nativeHandle);

    /// Detach from the native window
    void detach();

    /// Resize the editor
    void setSize(int width, int height);

    /// Is the editor currently attached?
    bool isAttached() const;

    // --- IParameterFinder access ---
    int findParameterAtPoint(int x, int y) const;

private:
    class PlugFrame;  // implements IPlugFrame

    Steinberg::IPlugView* view_ = nullptr;
    PluginInstance& instance_;
    std::unique_ptr<PlugFrame> frame_;
    bool attached_ = false;
};

} // namespace dc
```

### IPlugFrame Implementation

```cpp
class PluginEditor::PlugFrame : public Steinberg::IPlugFrame
{
public:
    /// Called when plugin requests a resize
    tresult resizeView(IPlugView* view,
                       Steinberg::ViewRect* newSize) override;

    /// Callback to notify host of resize request
    void setResizeCallback(std::function<void(int, int)> cb);

private:
    std::function<void(int, int)> resizeCallback_;
};
```

### Platform Window Embedding

The existing platform bridges (`MacPluginEditorBridge`, `X11PluginEditorBridge`)
continue to handle the native window management. They change from receiving a
`juce::AudioProcessorEditor*` to receiving a `dc::PluginEditor*`:

```cpp
// Before
auto* juceEditor = plugin->createEditor();
bridge->embed(juceEditor);

// After
auto editor = pluginInstance->createEditor();
bridge->embed(editor.get());
```

The bridge calls `editor->attachToWindow(nativeHandle)` with the platform
window handle (NSView* on macOS, XID on Linux).

---

## Design: `dc::VST3Host`

Top-level host class that ties everything together.

```cpp
namespace dc {

class VST3Host
{
public:
    VST3Host();
    ~VST3Host();

    /// Scan for plugins (out-of-process)
    void scanPlugins(PluginScanner::ProgressCallback cb = {});

    /// Get the plugin database
    const std::vector<PluginDescription>& getKnownPlugins() const;

    /// Load/save plugin database (YAML)
    void loadDatabase(const std::filesystem::path& path);
    void saveDatabase(const std::filesystem::path& path) const;

    /// Create a plugin instance (async)
    using CreateCallback = std::function<void(
        std::unique_ptr<PluginInstance> instance,
        std::string error)>;
    void createInstance(const PluginDescription& desc,
                        double sampleRate, int maxBlockSize,
                        CreateCallback callback);

    /// Create a plugin instance (sync, for offline use)
    std::unique_ptr<PluginInstance> createInstanceSync(
        const PluginDescription& desc,
        double sampleRate, int maxBlockSize);

private:
    PluginScanner scanner_;
    std::vector<PluginDescription> knownPlugins_;
    std::unordered_map<std::string, std::unique_ptr<VST3Module>> loadedModules_;
};

} // namespace dc
```

---

## State Management

### Save

```cpp
auto state = pluginInstance->getState();
auto base64 = dc::base64Encode(state);
tree.setProperty(IDs::pluginState, dc::Variant(base64), undoManager);
```

### Restore

```cpp
auto base64 = tree.getProperty(IDs::pluginState).toString();
auto state = dc::base64Decode(base64);
pluginInstance->setState(state);
```

The binary format is identical to what JUCE uses internally — the VST3
`IComponent::getState()` / `IComponent::setState()` produce/consume the
same binary blob regardless of whether JUCE or our host calls them.
Existing sessions load without conversion.

---

## IParameterFinder Integration

With direct VST3 hosting, `IParameterFinder` is accessed natively:

```cpp
// Query the controller for IParameterFinder
auto* finder = FUnknownPtr<IParameterFinder>(controller_);
if (finder)
{
    Steinberg::Vst::ParamID resultId;
    if (finder->findParameter(x, y, resultId) == kResultOk)
        return getParameterIndex(resultId);
}
```

No JUCE patch needed. The `ParameterFinderScanner` changes from calling
a patched JUCE method to calling `pluginInstance->findParameterAtPoint(x, y)`.

---

## Migration Path

### Step 1: Add VST3 SDK dependency

Add Steinberg VST3 SDK to `libs/` (or use FetchContent). Update CMakeLists.txt.

```cmake
add_subdirectory(libs/vst3sdk)
target_link_libraries(DremCanvas PRIVATE sdk)
```

### Step 2: Implement dc::VST3Module

Module loading with `dlopen` / `GetPluginFactory`. Test with a known plugin.

### Step 3: Implement dc::PluginDescription + dc::PluginScanner

Enumerate plugins from factory. Out-of-process scanning.

### Step 4: Implement dc::PluginInstance

Core hosting: create component, setup processing, process audio blocks.
Implement `AudioNode` interface.

### Step 5: Implement dc::ComponentHandler

Host-side handler for parameter edits, restart requests. Include
performEdit snoop queue.

### Step 6: Implement dc::PluginEditor

IPlugView lifecycle, IPlugFrame for resize. Connect to existing platform
bridges.

### Step 7: Migrate PluginManager

Replace `KnownPluginList` with `VST3Host` database. Update YAML persistence.

### Step 8: Migrate PluginHost

Replace `createPluginInstanceAsync` with `VST3Host::createInstance`. Update
state save/restore.

### Step 9: Migrate ParameterFinderScanner

Replace JUCE-patched spatial hints with native `IParameterFinder` access.
Remove performEdit snoop patch — use `ComponentHandler` directly.

### Step 10: Delete JUCE patches

Remove `scripts/juce-patches/` and related bootstrap code.

---

## Files Affected

| File | Key changes |
|------|------------|
| `src/plugins/PluginManager.h/.cpp` | `KnownPluginList` → `VST3Host` database |
| `src/plugins/PluginHost.h/.cpp` | `AudioPluginInstance` → `PluginInstance` |
| `src/plugins/PluginWindowManager.h/.cpp` | `AudioProcessorEditor` → `PluginEditor` |
| `src/plugins/PluginEditorBridge.h/.cpp` | Update bridge interface |
| `src/plugins/ParameterFinderScanner.h/.cpp` | Native IParameterFinder, native snoop |
| `src/plugins/SyntheticInputProbe.h/.cpp` | No JUCE types needed |
| `src/plugins/VST3ParameterFinderSupport.h` | Simplify (direct SDK access) |
| `src/platform/MacPluginEditorBridge.h/.mm` | `AudioProcessorEditor` → `PluginEditor` |
| `src/platform/linux/X11PluginEditorBridge.h/.cpp` | Same |
| `src/platform/linux/EmbeddedPluginEditor.h/.cpp` | Same |
| `src/engine/TrackProcessor.h/.cpp` | Plugin chain: `AudioPluginInstance` → `PluginInstance` |
| `scripts/juce-patches/` | Delete entirely |
| `scripts/bootstrap.sh` | Remove JUCE patch application code |

## Testing Strategy

1. **Module loading**: Load a known VST3 bundle, verify factory enumeration
2. **Scanning**: Scan default directories, compare results with JUCE scan
3. **Instantiation**: Create instance, verify `prepare()` / `process()` works
4. **Audio processing**: Process silence → verify no crash. Process sine → verify output
5. **Parameters**: Enumerate parameters, set/get values, verify range
6. **State round-trip**: Save state → new instance → restore → verify identical output
7. **Editor**: Open editor, verify native window embedding works
8. **IParameterFinder**: Query known coordinates, verify correct parameter returned
9. **performEdit snoop**: Interact with plugin UI, verify edit events captured
10. **yabridge**: Test with yabridge-bridged Windows VST3 plugins
11. **Session compatibility**: Load existing YAML sessions with plugin state
