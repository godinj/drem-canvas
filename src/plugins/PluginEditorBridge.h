#pragma once

#include <memory>
#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

namespace dc { class PluginInstance; class PluginEditor; }

namespace dc
{

/**
 * Abstract interface for hosting a plugin editor and capturing its pixels.
 *
 * Combines editor lifecycle management (open/close/position) with pixel
 * capture for Skia compositing. Each platform provides its own implementation:
 *   - Linux: X11PluginEditorBridge (EmbeddedPluginEditor + X11Compositor)
 *   - macOS: MacPluginEditorBridge (CGWindowListCreateImage)
 *   - Windows: Win32PluginEditorBridge (future)
 */
class PluginEditorBridge
{
public:
    virtual ~PluginEditorBridge() = default;

    // Editor lifecycle
    virtual void openEditor (dc::PluginInstance* plugin) = 0;
    virtual void closeEditor() = 0;
    virtual bool isOpen() const = 0;

    // Editor geometry (native/unscaled dimensions)
    virtual int getNativeWidth() const = 0;
    virtual int getNativeHeight() const = 0;

    /** Position and scale the editor within the given bounds.
        The implementation handles reparenting, scaling, and off-screen placement
        as appropriate for the platform. */
    virtual void setTargetBounds (int x, int y, int w, int h) = 0;

    // Pixel capture for Skia compositing

    /** Returns true if the editor has new content since the last capture. */
    virtual bool hasDamage() = 0;

    /** Capture the editor pixels as an SkImage.
        Returns a cached image if no damage since the last capture. */
    virtual sk_sp<SkImage> capture() = 0;

    /** Returns true if compositing is active (pixel capture is available). */
    virtual bool isCompositing() const = 0;

    /** Access the underlying dc::PluginEditor (for ParameterFinderScanner). */
    virtual dc::PluginEditor* getEditor() const = 0;

    /** Content scale factor (logical points -> physical pixels).
        Used by MouseEventForwarder to convert widget-space deltas
        to X11 root-pixel deltas for XTest injection. */
    virtual float getContentScale() const { return 1.0f; }

    /** Factory: create the platform-appropriate bridge implementation.
        @param nativeWindowHandle  Platform-specific parent window handle
               (GLFWwindow* on Linux, NSWindow* on macOS). */
    static std::unique_ptr<PluginEditorBridge> create (void* nativeWindowHandle);
};

} // namespace dc
