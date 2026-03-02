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
 * Embeds a dc::PluginEditor inside the GLFW main window via X11.
 *
 * On X11: the IPlugView creates an X11 child window which we reparent
 * into the GLFW window via XReparentWindow.
 *
 * On Wayland: X11 reparenting isn't possible across the Wayland/XWayland
 * boundary, so the editor is shown as a floating XWayland window.
 *
 * The editor is scaled to fit within the available panel area while
 * preserving aspect ratio.
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
