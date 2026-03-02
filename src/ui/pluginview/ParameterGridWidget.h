#pragma once

#include "graphics/core/Widget.h"
#include "vim/VimContext.h"
#include "vim/VimEngine.h"
#include <JuceHeader.h>
#include <string>
#include <unordered_map>
#include <vector>

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
    void setHintBuffer (const std::string& buffer);
    void setNumberEntryActive (bool active);
    void setNumberBuffer (const std::string& buffer);

    /** Set spatial hint labels keyed by JUCE parameter index.
        When non-empty, these labels are shown instead of generated hints. */
    void setSpatialHintMap (std::unordered_map<int, std::string> map);

    int getNumParameters() const;

private:
    juce::AudioPluginInstance* currentPlugin = nullptr;
    std::vector<juce::AudioProcessorParameter*> parameters;

    int selectedParam = 0;
    VimContext::HintMode hintMode = VimContext::HintNone;
    std::string hintBuffer;
    bool numberEntry = false;
    std::string numberBuffer;
    std::unordered_map<int, std::string> spatialHintMap;

    static constexpr float rowHeight = 24.0f;
    static constexpr float hintColWidth = 36.0f;
    static constexpr float nameColWidth = 180.0f;
    static constexpr float barColWidth = 120.0f;

    ParameterGridWidget (const ParameterGridWidget&) = delete;
    ParameterGridWidget& operator= (const ParameterGridWidget&) = delete;
};

} // namespace ui
} // namespace dc
