#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace gfx
{

class ScrollViewWidget : public Widget
{
public:
    ScrollViewWidget();

    void paint (Canvas& canvas) override;
    void paintOverChildren (Canvas& canvas) override;
    void resized() override;

    bool mouseWheel (const WheelEvent& e) override;

    // Content management
    void setContentWidget (Widget* content);
    Widget* getContentWidget() const { return contentWidget; }

    void setContentSize (float w, float h);
    float getContentWidth() const { return contentWidth; }
    float getContentHeight() const { return contentHeight; }

    // Scroll position
    void setScrollOffset (float x, float y);
    float getScrollOffsetX() const { return scrollX; }
    float getScrollOffsetY() const { return scrollY; }

    void scrollToMakeVisible (const Rect& area);

    // Configuration
    void setShowHorizontalScrollbar (bool show) { showHScrollbar = show; repaint(); }
    void setShowVerticalScrollbar (bool show) { showVScrollbar = show; repaint(); }

private:
    void clampScrollOffset();
    void paintScrollbar (Canvas& canvas, bool horizontal);

    Widget* contentWidget = nullptr;
    float contentWidth = 0.0f;
    float contentHeight = 0.0f;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    bool showHScrollbar = true;
    bool showVScrollbar = true;
};

} // namespace gfx
} // namespace dc
