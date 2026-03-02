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

    // Get the native X11 window handle from GLFW for embedding
    unsigned long parentX11Window = 0;
    if (x11::isX11())
    {
        parentX11Window = x11::getWindow (parentWindow);
        xDisplay = x11::getDisplay();
    }

    if (parentX11Window != 0)
    {
        // X11: attach the IPlugView to the GLFW parent window.
        // The IPlugView creates a child X11 window under the parent.
        editor_->attachToWindow (reinterpret_cast<void*> (parentX11Window));
        reparented = true;

        // The IPlugView should have created an X11 window; we need to discover it.
        // The plug view's window becomes a child of parentX11Window.
        // For now, store the parent as the editor window — the compositor
        // will capture the child window via XComposite.
        // TODO: discover the actual child window created by IPlugView
        editorXWindow = parentX11Window;

        dc_log ("[EmbedEditor] X11 child window: parent=%lu", parentX11Window);
    }
    else
    {
        // Wayland fallback: attach to an independent X11 connection.
        // The IPlugView creates its own XWayland window.
        xDisplay = x11::openDisplay();
        if (xDisplay != nullptr)
        {
            // Create a small container window for the IPlugView
            editorXWindow = x11::createWindow (xDisplay, nativeWidth, nativeHeight);
            if (editorXWindow != 0)
            {
                editor_->attachToWindow (reinterpret_cast<void*> (editorXWindow));
                dc_log ("[EmbedEditor] Wayland mode: XWayland window %lu display=%p",
                        editorXWindow, xDisplay);
            }
        }
        else
        {
            dc_log ("[EmbedEditor] Wayland mode: could not open display");
        }
    }
}

void EmbeddedPluginEditor::closeEditor()
{
    if (editor_ != nullptr)
    {
        editor_->detach();
        editor_.reset();
    }

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
        x11::moveResizeWindow (xDisplay, editorXWindow, offsetX, offsetY, scaledW, scaledH);
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
        x11::moveResizeWindow (xDisplay, editorXWindow, screenX, screenY, w, h);
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
