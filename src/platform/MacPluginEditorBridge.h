#pragma once

#if defined(__APPLE__)

#include "plugins/PluginEditorBridge.h"
#include <JuceHeader.h>

namespace dc
{

/**
 * macOS implementation of PluginEditorBridge.
 * Uses CGWindowListCreateImage to capture the plugin editor window pixels
 * for Skia compositing.
 */
class MacPluginEditorBridge : public PluginEditorBridge
{
public:
    explicit MacPluginEditorBridge (void* nativeWindowHandle);
    ~MacPluginEditorBridge() override;

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

private:
    void* nsWindow = nullptr;  // NSWindow* of the main app window
    std::unique_ptr<juce::Component> holder;
    juce::AudioProcessorEditor* editor = nullptr;
    int nativeWidth = 0;
    int nativeHeight = 0;
    uint32_t cgWindowId = 0;
    sk_sp<SkImage> cachedImage;
    bool damaged = true;
};

} // namespace dc

#endif // __APPLE__
