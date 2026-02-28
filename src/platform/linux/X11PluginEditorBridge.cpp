#include "X11PluginEditorBridge.h"

#if defined(__linux__)

#include "platform/linux/EmbeddedPluginEditor.h"
#include "platform/linux/X11Compositor.h"
#include "platform/linux/X11Reparent.h"
#include <JuceHeader.h>

namespace dc
{

X11PluginEditorBridge::X11PluginEditorBridge (void* nativeWindowHandle)
    : glfwWindow (static_cast<GLFWwindow*> (nativeWindowHandle))
{
    embeddedEditor = std::make_unique<platform::EmbeddedPluginEditor>();
    compositor = std::make_unique<platform::x11::Compositor>();
}

X11PluginEditorBridge::~X11PluginEditorBridge()
{
    if (compositor)
        compositor->stopRedirect();
    embeddedEditor.reset();
}

void X11PluginEditorBridge::openEditor (juce::AudioPluginInstance* plugin)
{
    if (compositor)
        compositor->stopRedirect();
    compositorActive = false;

    if (embeddedEditor)
    {
        embeddedEditor->closeEditor();
        if (plugin != nullptr && glfwWindow != nullptr)
        {
            embeddedEditor->openEditor (plugin, glfwWindow);

            // Start compositor redirect BEFORE setTargetBounds.
            // The editor is still at its native size right after openEditor,
            // so the compositor acquires a full-resolution pixmap. Calling
            // setTargetBounds first would scale the editor down (possibly
            // to 1x1 if the widget hasn't been sized yet), and the compositor
            // would capture a tiny window that never recovers.
            if (compositor
                && embeddedEditor->getXDisplay() != nullptr
                && embeddedEditor->getXWindow() != 0)
            {
                compositorActive = compositor->startRedirect (
                    embeddedEditor->getXDisplay(),
                    embeddedEditor->getXWindow());

                // On native X11, hide the floating window so only the
                // composited Skia image is shown. On XWayland we must NOT
                // hide it — moving the window off-screen causes the Wayland
                // compositor to skip rendering, leaving the pixmap blank.
                if (compositorActive && embeddedEditor->isReparented())
                    compositor->hideWindow();
            }
        }
    }
}

void X11PluginEditorBridge::closeEditor()
{
    if (compositor)
        compositor->stopRedirect();
    compositorActive = false;
    if (embeddedEditor)
        embeddedEditor->closeEditor();
}

bool X11PluginEditorBridge::isOpen() const
{
    return embeddedEditor && embeddedEditor->isOpen();
}

int X11PluginEditorBridge::getNativeWidth() const
{
    return embeddedEditor ? embeddedEditor->getNativeWidth() : 0;
}

int X11PluginEditorBridge::getNativeHeight() const
{
    return embeddedEditor ? embeddedEditor->getNativeHeight() : 0;
}

void X11PluginEditorBridge::setTargetBounds (int x, int y, int w, int h)
{
    if (! embeddedEditor || ! embeddedEditor->isOpen())
        return;

    if (compositorActive && ! embeddedEditor->isReparented())
    {
        // Wayland + compositor: keep the editor at native size for full-res
        // capture. Position is off-screen; Skia handles the scaling for display.
        int nativeW = embeddedEditor->getNativeWidth();
        int nativeH = embeddedEditor->getNativeHeight();
        if (nativeW > 0 && nativeH > 0)
            embeddedEditor->setBounds (-10000, -10000, nativeW, nativeH);

        if (compositor)
            compositor->handleResize();
    }
    else if (w > 0 && h > 0)
    {
        if (embeddedEditor->isReparented())
        {
            // X11: coordinates are relative to the GLFW parent window.
            // setBounds scales the editor and anchors it bottom-right.
            embeddedEditor->setBounds (x, y, w, h);

            if (compositorActive && compositor)
                compositor->handleResize();
        }
        else
        {
            // Wayland without compositor: convert to absolute screen coords,
            // then setBounds handles scaling and bottom-right anchoring.
            int winX = 0, winY = 0;
            platform::x11::getWindowPos (glfwWindow, winX, winY);
            embeddedEditor->setBounds (winX + x, winY + y, w, h);
        }
    }
}

bool X11PluginEditorBridge::hasDamage()
{
    return compositor && compositor->hasDamage();
}

sk_sp<SkImage> X11PluginEditorBridge::capture()
{
    if (! compositorActive || ! compositor)
        return nullptr;

    return compositor->capture();
}

bool X11PluginEditorBridge::isCompositing() const
{
    return compositorActive;
}

juce::AudioProcessorEditor* X11PluginEditorBridge::getEditor() const
{
    return embeddedEditor ? embeddedEditor->getEditor() : nullptr;
}

void* X11PluginEditorBridge::getXDisplay() const
{
    return embeddedEditor ? embeddedEditor->getXDisplay() : nullptr;
}

unsigned long X11PluginEditorBridge::getXWindow() const
{
    return embeddedEditor ? embeddedEditor->getXWindow() : 0;
}

bool X11PluginEditorBridge::isReparented() const
{
    return embeddedEditor && embeddedEditor->isReparented();
}

} // namespace dc

#endif // __linux__
