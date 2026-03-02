#pragma once

#include "graphics/core/Widget.h"
#include "ParameterGridWidget.h"
#include "vim/VimContext.h"
#include "plugins/ParameterFinderScanner.h"
#include <JuceHeader.h>
#include <string>

namespace dc
{

class PluginEditorBridge;
class SyntheticInputProbe;
class SyntheticMouseDrag;
class MouseEventForwarder;

namespace ui
{

class PluginViewWidget : public gfx::Widget
{
public:
    PluginViewWidget();
    ~PluginViewWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void resized() override;

    // Mouse event forwarding to composited plugin editor
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;
    void mouseUp (const gfx::MouseEvent& e) override;
    void mouseMove (const gfx::MouseEvent& e) override;
    bool mouseWheel (const gfx::WheelEvent& e) override;

    void setPlugin (juce::AudioPluginInstance* plugin, const std::string& pluginName,
                    const std::string& fileOrIdentifier = {});
    void clearPlugin();

    void setActiveContext (bool active);

    // Forward state from VimContext
    void setSelectedParamIndex (int index);
    void setHintMode (VimContext::HintMode mode);
    void setHintBuffer (const std::string& buffer);
    void setNumberEntryActive (bool active);
    void setNumberBuffer (const std::string& buffer);

    int getNumParameters() const;

    void setEnlarged (bool enlarged);
    bool isEnlarged() const { return enlarged; }

    // Spatial hint support (scan runs lazily on first query)
    void runSpatialScan();
    void forceSpatialRescan();
    bool hasSpatialHints();
    bool isSpatialMode() const;
    const std::vector<SpatialParamInfo>& getSpatialResults() const { return spatialScanner.getResults(); }

    void setEditorBridge (std::unique_ptr<PluginEditorBridge> bridge);
    void updateEditorBounds();

    // Synthetic mouse drag for vim parameter adjustment (h/l/H/L)
    /** Apply a synthetic mouse drag for the given parameter.
        In spatial mode, paramIndex is a spatial index; otherwise it's a JUCE param index.
        Returns true if the drag was sent (parameter has spatial data). */
    bool applyMouseDrag (int paramIndex, int pixelDelta);

    /** Sweep-and-position drag for absolute value setting on unmapped params.
        Drags down 300px (reset to ~0%), then up by value*300px.
        Returns true if the drag was sent. */
    bool applyAbsoluteDrag (int spatialIndex, float value);

    /** End any active synthetic mouse drag session. */
    void endMouseDrag();

    /** Toggle drag axis between horizontal (default) and vertical. */
    void toggleDragAxis();
    bool isDragHorizontal() const { return dragHorizontal; }

    /** Toggle whether direction changes (h↔l) reset to centroid first. */
    void toggleDragCenterOnReverse();
    bool isDragCenterOnReverse() const { return dragCenterOnReverse; }

private:
    ParameterGridWidget paramGrid;
    std::string pluginName;
    std::string pluginFileOrIdentifier;
    bool activeContext = false;
    juce::AudioPluginInstance* currentPlugin = nullptr;

    bool enlarged = false;
    static constexpr float headerHeight = 30.0f;

    // Spatial hint state
    ParameterFinderScanner spatialScanner;
    bool spatialScanComplete = false;
    bool spatialHintMode = false;
    std::string spatialHintBuffer;

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
    std::unique_ptr<SyntheticMouseDrag> syntheticDrag;
    std::unique_ptr<MouseEventForwarder> mouseForwarder;
    unsigned int forwardedButtonMask = 0;  // bitmask of buttons held during forwarding
    float forwardStartX = 0, forwardStartY = 0;  // widget-space origin of forwarded drag

    struct DragSession
    {
        bool active = false;
        int paramIndex = -1;
        int originX = 0;
        int originY = 0;
        int currentDeltaX = 0;
        int currentDeltaY = 0;
        int lastDragSign = 0;        // +1 or -1, tracks last movement direction
    };
    DragSession dragSession;
    bool dragHorizontal = true;      // true = X-axis drag (default), false = Y-axis drag
    bool dragCenterOnReverse = true; // true = reset to centroid when direction reverses
    bool dragSuppressBounds = false;  // Suppress updateEditorBounds during XTest drag

    bool getSpatialCentroid (int paramIndex, int& cx, int& cy) const;
    bool getSpatialCentroidBySpatialIndex (int spatialIdx, int& cx, int& cy) const;

    /** Transform widget-local mouse coordinates to native plugin coordinates.
        Returns false if the point is outside the composited image area. */
    bool widgetToNativeCoords (float widgetX, float widgetY, int& nativeX, int& nativeY) const;

    PluginViewWidget (const PluginViewWidget&) = delete;
    PluginViewWidget& operator= (const PluginViewWidget&) = delete;
};

} // namespace ui
} // namespace dc
