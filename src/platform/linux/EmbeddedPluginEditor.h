#pragma once

#if defined(__linux__)

#include <JuceHeader.h>

struct GLFWwindow;

namespace dc
{
namespace platform
{

/**
 * Embeds a JUCE AudioProcessorEditor inside the GLFW main window.
 *
 * On X11: reparents the JUCE editor window into the GLFW window via
 * XReparentWindow, positioning it in the right panel area.
 *
 * On Wayland: X11 reparenting isn't possible across the Wayland/XWayland
 * boundary, so the editor is shown as a floating XWayland window.
 *
 * The editor is scaled via JUCE's setScaleFactor() to fit within the
 * available panel area while preserving aspect ratio.
 */
class EmbeddedPluginEditor
{
public:
    EmbeddedPluginEditor();
    ~EmbeddedPluginEditor();

    void openEditor (juce::AudioPluginInstance* plugin, GLFWwindow* parentWindow);
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

    juce::AudioProcessorEditor* getEditor() const { return editor; }

    void* getXDisplay() const { return xDisplay; }
    unsigned long getXWindow() const { return editorXWindow; }

private:
    std::unique_ptr<juce::Component> holder;
    juce::AudioProcessorEditor* editor = nullptr;
    void* xDisplay = nullptr;           // X11 Display*
    unsigned long editorXWindow = 0;    // X11 Window
    bool reparented = false;
    int nativeWidth = 0;                // Editor's native (unscaled) size
    int nativeHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmbeddedPluginEditor)
};

} // namespace platform
} // namespace dc

#endif // __linux__
