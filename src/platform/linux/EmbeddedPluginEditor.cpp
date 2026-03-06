#if defined(__linux__)

#include "EmbeddedPluginEditor.h"
#include "X11Reparent.h"
#include "dc/foundation/assert.h"
#include <algorithm>

namespace dc
{
namespace platform
{

EmbeddedPluginEditor::EmbeddedPluginEditor()
{
}

EmbeddedPluginEditor::~EmbeddedPluginEditor()
{
    closeEditor();
}

void EmbeddedPluginEditor::openEditor (dc::PluginInstance* plugin, GLFWwindow* parentWindow)
{
    if (plugin == nullptr || parentWindow == nullptr)
        return;

    closeEditor();

    editor_ = plugin->createEditor();
    if (editor_ == nullptr)
    {
        dc_log ("[EmbedEditor] createEditor returned null");
        return;
    }

    // Store the editor's native (unscaled) size
    auto [w, h] = editor_->getPreferredSize();
    nativeWidth = w;
    nativeHeight = h;

    dc_log ("[EmbedEditor] editor native size: %dx%d", nativeWidth, nativeHeight);

    // Get or open an X11 display connection.
    // X11: borrow GLFW's display (reparented = true → don't close it).
    // Wayland: open an independent display for XWayland hosting.
    if (x11::isX11())
    {
        xDisplay = x11::getDisplay();
        reparented = true;
    }
    else
    {
        xDisplay = x11::openDisplay();
    }

    if (xDisplay == nullptr)
    {
        dc_log ("[EmbedEditor] could not get/open X11 display");
        return;
    }

    // Create a dedicated off-screen container window.  It starts at
    // (-10000, -10000) with override_redirect so it never flashes on screen.
    // The IPlugView creates its child window inside this container, and the
    // X11Compositor captures pixels from it via XComposite.
    editorXWindow = x11::createWindow (xDisplay, nativeWidth, nativeHeight);
    if (editorXWindow != 0)
    {
        editor_->attachToWindow (reinterpret_cast<void*> (editorXWindow));
        dc_log ("[EmbedEditor] attached to off-screen container: window=%lu display=%p x11=%d",
                editorXWindow, xDisplay, reparented);
    }
}

void EmbeddedPluginEditor::closeEditor()
{
    if (editor_ != nullptr)
    {
        editor_->detach();
        editor_.reset();
    }

    // Destroy the container window we created
    x11::destroyWindow (xDisplay, editorXWindow);

    // Close our own X11 display if we opened one (Wayland mode)
    if (! reparented && xDisplay != nullptr)
        x11::closeDisplay (xDisplay);
    xDisplay = nullptr;
    editorXWindow = 0;
    reparented = false;
    nativeWidth = 0;
    nativeHeight = 0;
}

void EmbeddedPluginEditor::scaleToFit (int maxW, int maxH, int& outW, int& outH)
{
    if (editor_ == nullptr || nativeWidth <= 0 || nativeHeight <= 0
        || maxW <= 0 || maxH <= 0)
    {
        outW = std::max (maxW, 0);
        outH = std::max (maxH, 0);
        return;
    }

    float scaleX = static_cast<float> (maxW) / static_cast<float> (nativeWidth);
    float scaleY = static_cast<float> (maxH) / static_cast<float> (nativeHeight);
    float scale = std::min (scaleX, scaleY);

    // Don't upscale — only shrink if the editor is larger than the available area
    scale = std::min (scale, 1.0f);

    outW = static_cast<int> (nativeWidth * scale);
    outH = static_cast<int> (nativeHeight * scale);

    dc_log ("[EmbedEditor] scaleToFit: native=%dx%d max=%dx%d scale=%f result=%dx%d",
            nativeWidth, nativeHeight, maxW, maxH, scale, outW, outH);

    // Resize the editor via IPlugView
    editor_->setSize (outW, outH);
}

void EmbeddedPluginEditor::setBounds (int x, int y, int width, int height)
{
    if (! isOpen())
        return;

    // Scale the editor to fit within the given bounds
    int scaledW, scaledH;
    scaleToFit (width, height, scaledW, scaledH);

    // Anchor to bottom-right of the available area
    int offsetX = x + (width - scaledW);
    int offsetY = y + (height - scaledH);

    dc_log ("[EmbedEditor] setBounds: panel=(%d,%d %dx%d) scaled=%dx%d offset=(%d,%d)",
            x, y, width, height, scaledW, scaledH, offsetX, offsetY);

    // Move the X11 window
    if (editorXWindow != 0 && xDisplay != nullptr)
        x11::moveResize (xDisplay, editorXWindow, offsetX, offsetY, scaledW, scaledH);
}

void EmbeddedPluginEditor::setScreenPosition (int screenX, int screenY)
{
    if (! isOpen() || reparented)
        return;

    // Wayland: position the floating window at absolute screen coords
    if (editorXWindow != 0 && xDisplay != nullptr)
    {
        int w = nativeWidth > 0 ? nativeWidth : 400;
        int h = nativeHeight > 0 ? nativeHeight : 300;
        x11::moveResize (xDisplay, editorXWindow, screenX, screenY, w, h);
    }
}

bool EmbeddedPluginEditor::isOpen() const
{
    return editor_ != nullptr;
}

bool EmbeddedPluginEditor::isReparented() const
{
    return reparented;
}

int EmbeddedPluginEditor::getEditorWidth() const
{
    if (editor_ != nullptr)
    {
        auto [w, h] = editor_->getPreferredSize();
        return w;
    }
    return 0;
}

int EmbeddedPluginEditor::getEditorHeight() const
{
    if (editor_ != nullptr)
    {
        auto [w, h] = editor_->getPreferredSize();
        return h;
    }
    return 0;
}

} // namespace platform
} // namespace dc

#endif // __linux__
