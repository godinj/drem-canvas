#pragma once

#include "graphics/core/Widget.h"
#include "ParameterGridWidget.h"
#include "vim/VimContext.h"
#include "plugins/ParameterFinderScanner.h"
#include <JuceHeader.h>

namespace dc
{

class PluginEditorBridge;
class SyntheticInputProbe;

namespace ui
{

class PluginViewWidget : public gfx::Widget
{
public:
    PluginViewWidget();
    ~PluginViewWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void resized() override;

    void setPlugin (juce::AudioPluginInstance* plugin, const juce::String& pluginName);
    void clearPlugin();

    void setActiveContext (bool active);

    // Forward state from VimContext
    void setSelectedParamIndex (int index);
    void setHintMode (VimContext::HintMode mode);
    void setHintBuffer (const juce::String& buffer);
    void setNumberEntryActive (bool active);
    void setNumberBuffer (const juce::String& buffer);

    int getNumParameters() const;

    void setEnlarged (bool enlarged);
    bool isEnlarged() const { return enlarged; }

    // Spatial hint support (scan runs lazily on first query)
    void runSpatialScan();
    bool hasSpatialHints();
    const std::vector<SpatialParamInfo>& getSpatialResults() const { return spatialScanner.getResults(); }

    void setEditorBridge (std::unique_ptr<PluginEditorBridge> bridge);
    void updateEditorBounds();

private:
    ParameterGridWidget paramGrid;
    juce::String pluginName;
    bool activeContext = false;
    juce::AudioPluginInstance* currentPlugin = nullptr;

    bool enlarged = false;
    static constexpr float headerHeight = 30.0f;

    // Spatial hint state
    ParameterFinderScanner spatialScanner;
    bool spatialScanComplete = false;
    bool spatialHintMode = false;
    juce::String spatialHintBuffer;

    struct CompositeGeometry
    {
        float drawX = 0, drawY = 0;
        float drawW = 0, drawH = 0;
        float scaleX = 1.0f, scaleY = 1.0f;
        bool valid = false;
    };
    CompositeGeometry computeCompositeGeometry() const;

    std::unique_ptr<PluginEditorBridge> editorBridge;
    std::unique_ptr<SyntheticInputProbe> inputProbe;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginViewWidget)
};

} // namespace ui
} // namespace dc
