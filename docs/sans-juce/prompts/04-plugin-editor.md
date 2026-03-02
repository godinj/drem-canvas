# Agent: PluginEditor — IPlugView Lifecycle + Window Embedding

You are working on the `feature/sans-juce-plugin-hosting` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 (VST3 Plugin Hosting): implement `dc::PluginEditor` to manage the
VST3 `IPlugView` lifecycle and platform window embedding, replacing the JUCE
`AudioProcessorEditor` wrapper.

## Context

Read these specs before starting:
- `docs/sans-juce/03-plugin-hosting.md` (PluginEditor, IPlugFrame sections)
- `src/dc/plugins/PluginInstance.h` (created by Agent 03 — provides `getController()`, `findParameterAtPoint()`)
- `src/plugins/PluginEditorBridge.h` (current abstract interface — note it uses `juce::AudioPluginInstance*` and `juce::AudioProcessorEditor*`)
- `src/platform/linux/EmbeddedPluginEditor.h` (current JUCE-based editor hosting)

## Dependencies

This agent depends on Agent 03 (PluginInstance + ComponentHandler). If `PluginInstance.h`
doesn't exist yet, create a stub header with the interface from
`docs/sans-juce/03-plugin-hosting.md` and implement against it.

## Deliverables

### New files (src/dc/plugins/)

#### 1. PluginEditor.h

Manages the VST3 `IPlugView` lifecycle and native window attachment.

```cpp
#pragma once
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/base/funknown.h>
#include <functional>
#include <memory>
#include <utility>

namespace dc {

class PluginInstance;

class PluginEditor
{
public:
    /// Create an editor from a plugin instance.
    /// Obtains IPlugView from the controller via createView("editor").
    /// Returns nullptr if the plugin has no editor.
    static std::unique_ptr<PluginEditor> create(PluginInstance& instance);

    ~PluginEditor();

    PluginEditor(const PluginEditor&) = delete;
    PluginEditor& operator=(const PluginEditor&) = delete;

    /// Get the editor's preferred size (width, height)
    std::pair<int, int> getPreferredSize() const;

    /// Attach to a native window handle.
    /// macOS: NSView*
    /// Linux: X11 Window (cast to void* from unsigned long / XID)
    /// The platform type is determined automatically.
    void attachToWindow(void* nativeHandle);

    /// Detach from the native window
    void detach();

    /// Resize the editor
    void setSize(int width, int height);

    /// Is the editor currently attached to a window?
    bool isAttached() const;

    /// Get the underlying IPlugView (for platform bridges that need direct access)
    Steinberg::IPlugView* getPlugView() const;

    /// Get the owning plugin instance
    PluginInstance& getInstance() const;

    // --- IParameterFinder delegation ---

    /// Find parameter at screen coordinates (relative to editor view).
    /// Returns parameter index, or -1 if not found or not supported.
    int findParameterAtPoint(int x, int y) const;

    // --- Resize callback ---

    /// Called when the plugin requests a resize via IPlugFrame.
    /// The callback receives (newWidth, newHeight).
    void setResizeCallback(std::function<void(int, int)> cb);

private:
    PluginEditor(Steinberg::IPlugView* view, PluginInstance& instance);

    class PlugFrame;  // implements IPlugFrame

    Steinberg::IPlugView* view_ = nullptr;
    PluginInstance& instance_;
    std::unique_ptr<PlugFrame> frame_;
    bool attached_ = false;
};

} // namespace dc
```

#### 2. PluginEditor.cpp

Implementation of IPlugView management.

**`create(instance)`:**
1. Get the controller from `instance.getController()`
2. If controller is null, return nullptr
3. Call `controller->createView("editor")` to get `IPlugView*`
4. If view is null, return nullptr
5. Construct `PluginEditor(view, instance)` via private constructor
6. Create the `PlugFrame` and set it on the view: `view->setFrame(frame_.get())`

**Constructor:**
- Store `view_` and `instance_` reference
- Create `PlugFrame` instance

**Destructor:**
- Call `detach()` if still attached
- `view_->setFrame(nullptr)` (unregister our frame)
- `view_->release()`

**`getPreferredSize()`:**
- Call `view_->getSize(&rect)` with `Steinberg::ViewRect rect`
- Return `{rect.right - rect.left, rect.bottom - rect.top}`

**`attachToWindow(nativeHandle)`:**
- Determine platform type:
  - macOS: `kPlatformTypeNSView`
  - Linux: `kPlatformTypeX11EmbedWindowID`
- Call `view_->attached(nativeHandle, platformType)`
- Set `attached_ = true`
- Use `#ifdef __APPLE__` / `#ifdef __linux__` for platform detection

**`detach()`:**
- If not attached, return
- Call `view_->removed()`
- Set `attached_ = false`

**`setSize(width, height)`:**
- Create `Steinberg::ViewRect rect{0, 0, width, height}`
- Call `view_->onSize(&rect)`

**`findParameterAtPoint(x, y)`:**
- Delegate to `instance_.findParameterAtPoint(x, y)`

**PlugFrame implementation:**

```cpp
class PluginEditor::PlugFrame : public Steinberg::IPlugFrame
{
public:
    Steinberg::tresult PLUGIN_API resizeView(
        Steinberg::IPlugView* view,
        Steinberg::ViewRect* newSize) override
    {
        if (!view || !newSize) return Steinberg::kInvalidArgument;

        int w = newSize->right - newSize->left;
        int h = newSize->bottom - newSize->top;

        // Apply the resize to the view
        auto result = view->onSize(newSize);

        // Notify host of the resize
        if (resizeCallback_)
            resizeCallback_(w, h);

        return result;
    }

    // FUnknown
    Steinberg::tresult PLUGIN_API queryInterface(
        const Steinberg::TUID iid, void** obj) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual(iid,
            Steinberg::IPlugFrame::iid))
        {
            addRef();
            *obj = static_cast<IPlugFrame*>(this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount_; }
    Steinberg::uint32 PLUGIN_API release() override { return --refCount_; }

    void setResizeCallback(std::function<void(int, int)> cb)
    {
        resizeCallback_ = std::move(cb);
    }

private:
    std::function<void(int, int)> resizeCallback_;
    std::atomic<Steinberg::uint32> refCount_{1};
};
```

**Important notes:**
- Do NOT delete PlugFrame via release() — it's owned by PluginEditor (prevent double-free).
  The ref-counting is for COM compliance but we manage lifetime explicitly.
- `kPlatformTypeX11EmbedWindowID` — on Linux, the `nativeHandle` passed to `attached()`
  is the X11 Window ID cast to `void*`. The VST3 SDK expects `FIDString` platform type.
- `kPlatformTypeNSView` — on macOS, `nativeHandle` is an `NSView*`.

### Modified files

#### 3. CMakeLists.txt

Add `src/dc/plugins/PluginEditor.cpp` to `target_sources` under `# dc::plugins library`.

## Scope Limitation

This agent ONLY creates the `PluginEditor` wrapper. Do NOT modify the existing platform
bridges (`MacPluginEditorBridge`, `X11PluginEditorBridge`, `EmbeddedPluginEditor`) or the
abstract `PluginEditorBridge` interface — those migrations are handled by Agent 06.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Use `dc_log()` from `dc/foundation/assert.h` for logging
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
