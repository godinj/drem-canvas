#pragma once

#if defined(__linux__)

#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"
#include <memory>

struct GLFWwindow;

namespace dc
{
namespace platform
{

/**
 * Hosts a dc::PluginEditor in a dedicated off-screen X11 container window.
 *
 * On X11: uses GLFW's display connection. The container starts at
 * (-10000, -10000) with override_redirect so it never appears on screen.
 *
 * On Wayland: opens an independent X11 display for XWayland. The
 * container is likewise off-screen.
 *
 * The X11Compositor captures pixels from this container via XComposite.
 */
class EmbeddedPluginEditor
{
public:
    EmbeddedPluginEditor();
    ~EmbeddedPluginEditor();

    void openEditor (dc::PluginInstance* plugin, GLFWwindow* parentWindow);
    void closeEditor();

    /** On X11: repositions within parent. On Wayland: sets screen position. */
    void setBounds (int x, int y, int width, int height);

    /** Wayland only: position the floating window at absolute screen coords. */
    void setScreenPosition (int screenX, int screenY);

    /** Scale the editor to fit within maxW x maxH, preserving aspect ratio.
        Returns the actual scaled dimensions via outW/outH. */
    void scaleToFit (int maxW, int maxH, int& outW, int& outH);

    bool isOpen() const;
    bool isReparented() const;

    int getEditorWidth() const;
    int getEditorHeight() const;

    /** Original unscaled editor dimensions (set once at open time). */
    int getNativeWidth() const { return nativeWidth; }
    int getNativeHeight() const { return nativeHeight; }

    dc::PluginEditor* getEditor() const { return editor_.get(); }

    void* getXDisplay() const { return xDisplay; }
    unsigned long getXWindow() const { return editorXWindow; }

private:
    std::unique_ptr<dc::PluginEditor> editor_;
    void* xDisplay = nullptr;           // X11 Display*
    unsigned long editorXWindow = 0;    // X11 Window
    bool reparented = false;
    int nativeWidth = 0;                // Editor's native (unscaled) size
    int nativeHeight = 0;

    EmbeddedPluginEditor (const EmbeddedPluginEditor&) = delete;
    EmbeddedPluginEditor& operator= (const EmbeddedPluginEditor&) = delete;
};

} // namespace platform
} // namespace dc

#endif // __linux__
