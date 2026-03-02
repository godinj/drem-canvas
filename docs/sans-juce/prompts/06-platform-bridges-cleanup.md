# Agent: Platform Bridges + ParameterFinder + TrackProcessor Migration + Cleanup

You are working on the `feature/sans-juce-plugin-hosting` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 (VST3 Plugin Hosting): migrate all remaining plugin-related files from
JUCE types to dc:: types. This includes platform editor bridges, the parameter finder system,
TrackProcessor's plugin chain, and cleanup of JUCE patches.

## Context

Read these specs before starting:
- `docs/sans-juce/03-plugin-hosting.md` (PluginEditor platform embedding, IParameterFinder, Migration Path steps 6, 9, 10)
- `docs/sans-juce/08-migration-guide.md` (Phase 4 — files to migrate, Phase 4 Verification)
- `src/plugins/PluginEditorBridge.h` (abstract bridge: uses `juce::AudioPluginInstance*`, `juce::AudioProcessorEditor*`)
- `src/plugins/PluginEditorBridge.cpp` (factory method)
- `src/plugins/PluginWindowManager.h` (uses `juce::DocumentWindow`, `juce::AudioPluginInstance`, `juce::AudioProcessorEditor`)
- `src/plugins/PluginWindowManager.cpp`
- `src/plugins/ParameterFinderScanner.h` (uses `juce::AudioPluginInstance*`)
- `src/plugins/ParameterFinderScanner.cpp`
- `src/plugins/VST3ParameterFinderSupport.h` (abstract interface for IParameterFinder)
- `src/plugins/SyntheticInputProbe.h` (already dc:: pure — verify no JUCE types)
- `src/plugins/SyntheticInputProbe.cpp`
- `src/platform/MacPluginEditorBridge.h` (uses `juce::AudioPluginInstance`, `juce::Component`, `juce::AudioProcessorEditor`)
- `src/platform/MacPluginEditorBridge.mm`
- `src/platform/linux/X11PluginEditorBridge.h` (uses `juce::AudioPluginInstance*`, `juce::AudioProcessorEditor*`)
- `src/platform/linux/X11PluginEditorBridge.cpp`
- `src/platform/linux/EmbeddedPluginEditor.h` (uses `juce::AudioPluginInstance*`, `juce::Component`, `juce::AudioProcessorEditor*`)
- `src/platform/linux/EmbeddedPluginEditor.cpp`
- `src/engine/TrackProcessor.h` (base class `juce::AudioProcessor`, plugin chain references)
- `src/engine/TrackProcessor.cpp`
- `src/dc/plugins/PluginEditor.h` (created by Agent 04 — the new dc:: editor wrapper)
- `src/dc/plugins/PluginInstance.h` (created by Agent 03)

## Dependencies

This agent depends on:
- Agent 03 (PluginInstance) — for `dc::PluginInstance` type
- Agent 04 (PluginEditor) — for `dc::PluginEditor` type
- Agent 05 (VST3Host + PluginManager/PluginHost migration) — for updated PluginManager/PluginHost interfaces

If those files don't exist yet, create stub headers matching the interfaces from
`docs/sans-juce/03-plugin-hosting.md`.

## Deliverables

### Migration

#### 1. src/plugins/PluginEditorBridge.h/.cpp

Replace JUCE types in the abstract bridge interface.

**Before:**
```cpp
// Forward declarations
namespace juce { class AudioPluginInstance; class AudioProcessorEditor; }

virtual void openEditor(juce::AudioPluginInstance* plugin) = 0;
virtual juce::AudioProcessorEditor* getEditor() const = 0;
```

**After:**
```cpp
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"

virtual void openEditor(dc::PluginInstance* plugin) = 0;
virtual dc::PluginEditor* getEditor() const = 0;
```

**Full interface after migration:**
```cpp
class PluginEditorBridge
{
public:
    virtual ~PluginEditorBridge() = default;

    virtual void openEditor(dc::PluginInstance* plugin) = 0;
    virtual void closeEditor() = 0;
    virtual bool isOpen() const = 0;

    virtual int getNativeWidth() const = 0;
    virtual int getNativeHeight() const = 0;
    virtual void setTargetBounds(int x, int y, int w, int h) = 0;

    virtual bool hasDamage() = 0;
    virtual sk_sp<SkImage> capture() = 0;
    virtual bool isCompositing() const = 0;

    virtual dc::PluginEditor* getEditor() const = 0;
    virtual float getContentScale() const { return 1.0f; }

    static std::unique_ptr<PluginEditorBridge> create(void* nativeWindowHandle);
};
```

**Remove:** `#include <JuceHeader.h>`, all `juce::` forward declarations.

#### 2. src/plugins/PluginWindowManager.h/.cpp

The `PluginWindowManager` currently uses `juce::DocumentWindow` to host plugin editors
in floating windows. With dc::PluginEditor, the editor is embedded via native window
handles (IPlugView → X11/NSView), not JUCE components.

**Approach**: Replace the JUCE DocumentWindow-based hosting with a lightweight manager
that tracks open editors by plugin instance pointer.

**Before:**
```cpp
#include <JuceHeader.h>
class PluginWindow : public juce::DocumentWindow { ... };
std::map<juce::AudioPluginInstance*, PluginWindow*> pluginToWindow;
void showEditorForPlugin(juce::AudioPluginInstance& plugin);
void closeEditorForPlugin(juce::AudioPluginInstance* plugin);
```

**After:**
```cpp
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"
#include <map>
#include <memory>

class PluginWindowManager
{
public:
    PluginWindowManager();
    ~PluginWindowManager();

    /// Show an editor for a plugin (creates dc::PluginEditor, attaches to window)
    void showEditorForPlugin(dc::PluginInstance& plugin);

    /// Close the editor for a plugin
    void closeEditorForPlugin(dc::PluginInstance* plugin);

    /// Close all editors
    void closeAll();

private:
    std::map<dc::PluginInstance*, std::unique_ptr<dc::PluginEditor>> editors_;
};
```

**Implementation notes:**
- `showEditorForPlugin`: Call `plugin.createEditor()`, store in `editors_` map.
  The editor is attached to a window by the platform bridge layer (not here).
  PluginWindowManager now just tracks lifetime.
- `closeEditorForPlugin`: Remove from map (unique_ptr destructor handles cleanup).
- Remove `PluginWindow` class entirely (no more `juce::DocumentWindow`).
- Remove `#include <JuceHeader.h>`.

#### 3. src/plugins/ParameterFinderScanner.h/.cpp

Replace JUCE plugin instance reference with `dc::PluginInstance`.

**Before:**
```cpp
#include <JuceHeader.h>
void scan(VST3ParameterFinderSupport& finder,
          juce::AudioPluginInstance* plugin,
          int nativeWidth, int nativeHeight,
          int gridStep = 8);
```

**After:**
```cpp
#include "dc/plugins/PluginInstance.h"

void scan(dc::PluginInstance* plugin,
          int nativeWidth, int nativeHeight,
          int gridStep = 8);
```

**Implementation changes:**
- Remove the `VST3ParameterFinderSupport& finder` parameter entirely — the scanner
  now calls `plugin->findParameterAtPoint(x, y)` directly (native IParameterFinder
  access through PluginInstance).
- Remove the `VST3ParameterFinderSupport` dependency.
- The grid scanning loop remains the same: iterate (x, y) positions at `gridStep`
  intervals, call `findParameterAtPoint()`, accumulate hit counts per parameter.
- For the performEdit snoop fallback: use `plugin->popLastEdit()` instead of the
  JUCE-patched `beginEditSnoop()` / `endEditSnoop()` methods.
- Update `SpatialParamInfo`:
  - `juceParamIndex` → `paramIndex` (rename, same meaning)
  - `paramId` stays as `unsigned int` (compatible with `Steinberg::Vst::ParamID`)
  - `name` populated from `plugin->getParameterName(paramIndex)`
- Remove `#include <JuceHeader.h>`.

#### 4. src/plugins/VST3ParameterFinderSupport.h

This interface abstracted away IParameterFinder behind JUCE. With direct VST3 SDK
access, it is no longer needed — `dc::PluginInstance` provides `findParameterAtPoint()`
and `popLastEdit()` directly.

**Action**: Delete this file entirely, or if other code still references it, convert it
to a thin compatibility shim that delegates to `dc::PluginInstance`. Check all `#include`
references and remove them.

Search for files that include `VST3ParameterFinderSupport.h` and update them to use
`dc::PluginInstance` directly.

#### 5. src/plugins/SyntheticInputProbe.h/.cpp

This file is already dc-pure (no JUCE types). Verify by checking:
- No `#include <JuceHeader.h>`
- No `juce::` types
- Uses `dc::PluginEditorBridge&` (already dc:: type)

If it references `juce::AudioPluginInstance` or similar anywhere, update to
`dc::PluginInstance`. Otherwise, no changes needed — just verify.

#### 6. src/platform/MacPluginEditorBridge.h/.mm

Replace JUCE types with dc:: equivalents.

**Before:**
```cpp
#include <JuceHeader.h>
void openEditor(juce::AudioPluginInstance* plugin) override;
juce::AudioProcessorEditor* getEditor() const override;
// Members:
std::unique_ptr<juce::Component> holder;
juce::AudioProcessorEditor* editor = nullptr;
```

**After:**
```cpp
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"
void openEditor(dc::PluginInstance* plugin) override;
dc::PluginEditor* getEditor() const override;
// Members:
std::unique_ptr<dc::PluginEditor> editor_;
```

**Implementation changes in .mm:**
- `openEditor(dc::PluginInstance* plugin)`:
  1. `editor_ = plugin->createEditor()` (returns `unique_ptr<dc::PluginEditor>`)
  2. Get preferred size: `auto [w, h] = editor_->getPreferredSize()`
  3. Store native dimensions
  4. Create NSView for hosting (or use existing window's content view)
  5. Call `editor_->attachToWindow(nsView)` (NSView* as void*)
  6. Set up resize callback: `editor_->setResizeCallback([this](int w, int h) { ... })`
- Remove `std::unique_ptr<juce::Component> holder` — no longer needed (no JUCE component tree)
- Remove `juce::ComponentPeer` usage — no longer needed
- Pixel capture via `CGWindowListCreateImage` stays the same (captures the native window)
- The editor window is now a plain NSView, not a JUCE component peer
- Remove `#include <JuceHeader.h>`

**Note**: The macOS bridge uses CGWindowListCreateImage to capture pixels. This requires
the plugin editor to be in a real NSWindow. `dc::PluginEditor::attachToWindow(nsView)`
handles the IPlugView attachment; the bridge still needs to manage the NSWindow that
contains the NSView. Keep the existing NSWindow management, just remove the JUCE
Component/ComponentPeer layer.

#### 7. src/platform/linux/EmbeddedPluginEditor.h/.cpp

Replace JUCE types with dc:: equivalents.

**Before:**
```cpp
#include <JuceHeader.h>
void openEditor(juce::AudioPluginInstance* plugin, GLFWwindow* parentWindow);
juce::AudioProcessorEditor* getEditor() const;
// Members:
std::unique_ptr<juce::Component> holder;
juce::AudioProcessorEditor* editor = nullptr;
```

**After:**
```cpp
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"
void openEditor(dc::PluginInstance* plugin, GLFWwindow* parentWindow);
dc::PluginEditor* getEditor() const;
// Members:
std::unique_ptr<dc::PluginEditor> editor_;
```

**Implementation changes:**
- `openEditor(dc::PluginInstance* plugin, GLFWwindow* parentWindow)`:
  1. `editor_ = plugin->createEditor()`
  2. Get preferred size for native dimensions
  3. Get the X11 Window (XID) from GLFW: `glfwGetX11Window(parentWindow)`
     or create a child X11 window for embedding
  4. Call `editor_->attachToWindow((void*)(uintptr_t)xWindow)` — cast XID to void*
  5. Set up resize callback
- X11 reparenting (`XReparentWindow`) stays the same — the IPlugView creates its own
  X11 window which we reparent into our container
- Remove `std::unique_ptr<juce::Component> holder` — no JUCE component tree
- Remove `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` macro
- Remove `#include <JuceHeader.h>`

**Important**: The IPlugView on Linux uses `kPlatformTypeX11EmbedWindowID`. When
`attachToWindow()` is called, the plugin creates an X11 window as a child of the
provided XID. The existing X11 reparenting and XComposite code in `X11Compositor`
should continue to work — just adapt to get the X11 window from dc::PluginEditor's
IPlugView instead of from a JUCE component peer.

#### 8. src/platform/linux/X11PluginEditorBridge.h/.cpp

Replace JUCE types with dc:: equivalents.

**Before:**
```cpp
void openEditor(juce::AudioPluginInstance* plugin) override;
juce::AudioProcessorEditor* getEditor() const override;
```

**After:**
```cpp
void openEditor(dc::PluginInstance* plugin) override;
dc::PluginEditor* getEditor() const override;
```

**Implementation changes:**
- `openEditor` delegates to `embeddedEditor_->openEditor(plugin, glfwWindow)`
  (EmbeddedPluginEditor handles the IPlugView attachment)
- `getEditor` returns `embeddedEditor_->getEditor()`
- The X11Compositor and X11MouseEventForwarder layers should continue to work —
  they operate on the X11 window, not on JUCE types
- Remove `#include <JuceHeader.h>` if present
- Update forward declarations from JUCE types to dc:: types

#### 9. src/engine/TrackProcessor.h/.cpp

Remove `juce::AudioProcessor` base class. This file is shared with Phase 3 (AudioGraph)
but for Phase 4 we specifically remove the plugin-related JUCE types.

**Note**: TrackProcessor currently extends `juce::AudioProcessor` — this base class
is a Phase 3 concern (migrating to `dc::AudioNode`). If Phase 3 hasn't been done yet,
keep the `juce::AudioProcessor` base class BUT update any plugin chain references:

**Plugin chain references to update:**
- Any `juce::AudioPluginInstance*` stored for per-track plugin chains → `dc::PluginInstance*`
- Any `juce::AudioProcessorEditor*` references → `dc::PluginEditor*`
- If TrackProcessor stores a vector of plugins, update the element type

**If TrackProcessor has NO plugin chain members** (plugins managed externally by
PluginManager/PluginHost), then the only changes are:
- Remove any `#include <JuceHeader.h>` references to JUCE plugin types
- Keep the `juce::AudioProcessor` base class (Phase 3 responsibility)
- Add a `// TODO: Phase 3 — migrate juce::AudioProcessor to dc::AudioNode` comment

Read the file carefully to determine which case applies.

#### 10. scripts/juce-patches/ (delete)

Delete the entire `scripts/juce-patches/` directory. These patches added
IParameterFinder and performEdit snoop support to JUCE's VST3 hosting code.
With direct VST3 SDK hosting, both features are native.

#### 11. scripts/bootstrap.sh (update)

Remove the JUCE patch application code from `bootstrap.sh`. Search for sections
that:
- Apply git patches to `libs/JUCE`
- Fetch patch commits from sibling worktrees
- Reference `scripts/juce-patches/*.patch`

Remove those sections. Keep all other bootstrap functionality (Skia setup, dependency
checks, CMake configuration, etc.).

### Phase 4 Verification

After all changes, run:

```bash
# Zero JUCE plugin types in src/
grep -rn "juce::AudioPluginInstance\|juce::AudioProcessorEditor\|juce::AudioPluginFormatManager\|juce::KnownPluginList\|juce::PluginDescription\|juce::PluginDirectoryScanner" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm"
# Should return zero hits

# Build succeeds
cmake --build --preset release
```

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Use `dc_log()` from `dc/foundation/assert.h` for logging
- Remove `#include <JuceHeader.h>` from every migrated file
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
