#pragma once

#if defined(__linux__)

#include "plugins/PluginEditorBridge.h"
#include <memory>

struct GLFWwindow;

namespace dc { namespace platform { class EmbeddedPluginEditor; } }
namespace dc { namespace platform { namespace x11 { class Compositor; } } }

namespace dc
{

/**
 * Linux implementation of PluginEditorBridge.
 * Wraps EmbeddedPluginEditor (JUCE editor hosting + X11 reparenting)
 * and X11Compositor (XComposite pixel capture).
 */
class X11PluginEditorBridge : public PluginEditorBridge
{
public:
    explicit X11PluginEditorBridge (void* nativeWindowHandle);
    ~X11PluginEditorBridge() override;

    void openEditor (juce::AudioPluginInstance* plugin) override;
    void closeEditor() override;
    bool isOpen() const override;

    int getNativeWidth() const override;
    int getNativeHeight() const override;

    void setTargetBounds (int x, int y, int w, int h) override;

    bool hasDamage() override;
    sk_sp<SkImage> capture() override;
    bool isCompositing() const override;

    juce::AudioProcessorEditor* getEditor() const override;

    // X11-specific accessors (used by X11SyntheticInputProbe)
    void* getXDisplay() const;
    unsigned long getXWindow() const;
    bool isReparented() const;

private:
    GLFWwindow* glfwWindow = nullptr;
    std::unique_ptr<platform::EmbeddedPluginEditor> embeddedEditor;
    std::unique_ptr<platform::x11::Compositor> compositor;
    bool compositorActive = false;
};

} // namespace dc

#endif // __linux__
