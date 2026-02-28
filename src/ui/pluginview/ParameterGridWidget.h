#pragma once

#include "graphics/core/Widget.h"
#include "vim/VimContext.h"
#include "vim/VimEngine.h"
#include <JuceHeader.h>

namespace dc
{
namespace ui
{

class ParameterGridWidget : public gfx::Widget
{
public:
    ParameterGridWidget();
    ~ParameterGridWidget() override;

    void paint (gfx::Canvas& canvas) override;

    void setPlugin (juce::AudioPluginInstance* plugin);
    void clearPlugin();

    void setSelectedParamIndex (int index);
    void setHintMode (VimContext::HintMode mode);
    void setHintBuffer (const juce::String& buffer);
    void setNumberEntryActive (bool active);
    void setNumberBuffer (const juce::String& buffer);

    int getNumParameters() const;

private:
    juce::AudioPluginInstance* currentPlugin = nullptr;
    juce::Array<juce::AudioProcessorParameter*> parameters;

    int selectedParam = 0;
    VimContext::HintMode hintMode = VimContext::HintNone;
    juce::String hintBuffer;
    bool numberEntry = false;
    juce::String numberBuffer;

    static constexpr float rowHeight = 24.0f;
    static constexpr float hintColWidth = 36.0f;
    static constexpr float nameColWidth = 180.0f;
    static constexpr float barColWidth = 120.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterGridWidget)
};

} // namespace ui
} // namespace dc
