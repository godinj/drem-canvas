#pragma once

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivstplugview.h>
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
    static std::unique_ptr<PluginEditor> create (PluginInstance& instance);

    ~PluginEditor();

    PluginEditor (const PluginEditor&) = delete;
    PluginEditor& operator= (const PluginEditor&) = delete;

    /// Get the editor's preferred size (width, height)
    std::pair<int, int> getPreferredSize() const;

    /// Attach to a native window handle.
    /// macOS: NSView*
    /// Linux: X11 Window (cast to void* from unsigned long / XID)
    /// The platform type is determined automatically.
    void attachToWindow (void* nativeHandle);

    /// Detach from the native window
    void detach();

    /// Resize the editor
    void setSize (int width, int height);

    /// Is the editor currently attached to a window?
    bool isAttached() const;

    /// Get the underlying IPlugView (for platform bridges that need direct access)
    Steinberg::IPlugView* getPlugView() const;

    /// Get the owning plugin instance
    PluginInstance& getInstance() const;

    // --- IParameterFinder delegation ---

    /// Find parameter at screen coordinates (relative to editor view).
    /// Returns parameter index, or -1 if not found or not supported.
    int findParameterAtPoint (int x, int y) const;

    // --- Resize callback ---

    /// Called when the plugin requests a resize via IPlugFrame.
    /// The callback receives (newWidth, newHeight).
    void setResizeCallback (std::function<void (int, int)> cb);

private:
    PluginEditor (Steinberg::IPlugView* view, PluginInstance& instance);

    class PlugFrame;  // implements IPlugFrame

    Steinberg::IPlugView* view_ = nullptr;
    PluginInstance& instance_;
    std::unique_ptr<PlugFrame> frame_;
    Steinberg::Vst::IParameterFinder* viewFinder_ = nullptr;  // queried from view
    bool attached_ = false;
};

} // namespace dc
