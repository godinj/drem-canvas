#if defined(__linux__)

#include "EmbeddedPluginEditor.h"
#include "X11Reparent.h"
#include <iostream>
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

void EmbeddedPluginEditor::openEditor (juce::AudioPluginInstance* plugin, GLFWwindow* parentWindow)
{
    if (plugin == nullptr || parentWindow == nullptr)
        return;

    closeEditor();

    editor = plugin->createEditorIfNeeded();
    if (editor == nullptr)
    {
        std::cerr << "[EmbedEditor] createEditorIfNeeded returned null\n";
        return;
    }

    // Store the editor's native (unscaled) size
    nativeWidth = editor->getWidth();
    nativeHeight = editor->getHeight();

    std::cerr << "[EmbedEditor] editor native size: "
              << nativeWidth << "x" << nativeHeight << "\n";

    // Create a holder component that wraps the editor
    holder = std::make_unique<juce::Component>();
    holder->setSize (nativeWidth, nativeHeight);
    holder->addAndMakeVisible (editor);

    // Get the native X11 window handle from GLFW for JUCE parenting
    unsigned long parentX11Window = 0;
    if (x11::isX11())
    {
        parentX11Window = x11::getWindow (parentWindow);
        xDisplay = x11::getDisplay();
    }

    if (parentX11Window != 0)
    {
        // X11: pass the GLFW window as native parent to JUCE.
        // JUCE creates the peer as a child window — coordinates are
        // automatically parent-relative, no manual reparenting needed.
        holder->addToDesktop (juce::ComponentPeer::windowIsTemporary,
                              reinterpret_cast<void*> (parentX11Window));
        reparented = true;

        auto* peer = holder->getPeer();
        if (peer != nullptr)
            editorXWindow = reinterpret_cast<unsigned long> (peer->getNativeHandle());

        std::cerr << "[EmbedEditor] X11 child window: parent=" << parentX11Window
                  << " child=" << editorXWindow << "\n";
    }
    else
    {
        // Wayland fallback: show as standalone floating window.
        // JUCE creates an XWayland window; we just position and show it.
        holder->addToDesktop (juce::ComponentPeer::windowIsTemporary);
        std::cerr << "[EmbedEditor] Wayland mode: floating window\n";
        holder->setVisible (true);
        holder->toFront (false);

        // Get the XWayland window handle for compositor use
        auto* peer = holder->getPeer();
        if (peer != nullptr)
        {
            editorXWindow = reinterpret_cast<unsigned long> (peer->getNativeHandle());
            // Open an independent X11 connection to XWayland
            xDisplay = x11::openDisplay();
            std::cerr << "[EmbedEditor] Wayland XWayland window: " << editorXWindow
                      << " display=" << xDisplay << "\n";
        }
    }
}

void EmbeddedPluginEditor::closeEditor()
{
    if (holder != nullptr)
    {
        if (editor != nullptr)
            holder->removeChildComponent (editor);

        holder->removeFromDesktop();
        holder.reset();
    }

    // Delete the editor so the plugin clears its active-editor reference.
    // Without this, createEditorIfNeeded() returns the stale editor on
    // reopen — it was removed from desktop and won't render correctly.
    delete editor;
    editor = nullptr;
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
    if (editor == nullptr || nativeWidth <= 0 || nativeHeight <= 0
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

    std::cerr << "[EmbedEditor] scaleToFit: native=" << nativeWidth << "x" << nativeHeight
              << " max=" << maxW << "x" << maxH
              << " scale=" << scale
              << " result=" << outW << "x" << outH << "\n";

    editor->setScaleFactor (scale);
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

    std::cerr << "[EmbedEditor] setBounds: panel=(" << x << "," << y << " " << width << "x" << height << ")"
              << " scaled=" << scaledW << "x" << scaledH
              << " offset=(" << offsetX << "," << offsetY << ")\n";

    // JUCE handles positioning natively — for reparented windows it uses
    // parent-relative coordinates automatically (no manual X11 calls needed).
    holder->setBounds (offsetX, offsetY, scaledW, scaledH);

    std::cerr << "[EmbedEditor] after setBounds: holder=("
              << holder->getX() << "," << holder->getY() << " "
              << holder->getWidth() << "x" << holder->getHeight() << ")";
    if (auto* peer = holder->getPeer())
        std::cerr << " peerScale=" << peer->getPlatformScaleFactor();
    std::cerr << "\n";
}

void EmbeddedPluginEditor::setScreenPosition (int screenX, int screenY)
{
    if (! isOpen() || reparented)
        return;

    // Wayland: position the floating window at absolute screen coords
    int scaledW = holder != nullptr ? holder->getWidth() : 400;
    int scaledH = holder != nullptr ? holder->getHeight() : 300;

    auto* peer = holder->getPeer();
    if (peer != nullptr)
        peer->setBounds (juce::Rectangle<int> (screenX, screenY, scaledW, scaledH), false);
}

bool EmbeddedPluginEditor::isOpen() const
{
    return editor != nullptr && holder != nullptr;
}

bool EmbeddedPluginEditor::isReparented() const
{
    return reparented;
}

int EmbeddedPluginEditor::getEditorWidth() const
{
    return editor != nullptr ? editor->getWidth() : 0;
}

int EmbeddedPluginEditor::getEditorHeight() const
{
    return editor != nullptr ? editor->getHeight() : 0;
}

} // namespace platform
} // namespace dc

#endif // __linux__
