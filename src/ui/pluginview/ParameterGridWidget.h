#pragma once

#include "graphics/core/Widget.h"
#include "vim/VimContext.h"
#include "vim/VimEngine.h"
#include "plugins/ParameterFinderScanner.h"
#include <JuceHeader.h>
#include <string>
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

    /** Switch to spatial results mode — grid shows spatially-discovered controls
        instead of the full JUCE parameter list. */
    void setSpatialResults (const std::vector<SpatialParamInfo>& results,
                            juce::AudioPluginInstance* plugin);
    void clearSpatialResults();

    int getNumParameters() const;

private:
    juce::AudioPluginInstance* currentPlugin = nullptr;
    std::vector<juce::AudioProcessorParameter*> parameters;

    int selectedParam = 0;
    VimContext::HintMode hintMode = VimContext::HintNone;
    std::string hintBuffer;
    bool numberEntry = false;
    std::string numberBuffer;

    // Spatial mode: show spatially-discovered controls instead of JUCE params
    std::vector<SpatialParamInfo> spatialResults;
    bool spatialMode = false;
    juce::AudioPluginInstance* pluginForValues = nullptr;

    static constexpr float rowHeight = 24.0f;
    static constexpr float hintColWidth = 36.0f;
    static constexpr float nameColWidth = 180.0f;
    static constexpr float barColWidth = 120.0f;

    ParameterGridWidget (const ParameterGridWidget&) = delete;
    ParameterGridWidget& operator= (const ParameterGridWidget&) = delete;
};

} // namespace ui
} // namespace dc
