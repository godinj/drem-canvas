#pragma once

#include <memory>
#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

namespace juce { class AudioPluginInstance; class AudioProcessorEditor; }

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
    virtual void openEditor (juce::AudioPluginInstance* plugin) = 0;
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

    /** Access the underlying JUCE editor (for ParameterFinderScanner). */
    virtual juce::AudioProcessorEditor* getEditor() const = 0;

    /** Factory: create the platform-appropriate bridge implementation.
        @param nativeWindowHandle  Platform-specific parent window handle
               (GLFWwindow* on Linux, NSWindow* on macOS). */
    static std::unique_ptr<PluginEditorBridge> create (void* nativeWindowHandle);
};

} // namespace dc
