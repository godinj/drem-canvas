#include "ScrollViewWidget.h"
#include "graphics/rendering/Canvas.h"
#include <algorithm>

namespace dc
{
namespace gfx
{

ScrollViewWidget::ScrollViewWidget()
{
}

void ScrollViewWidget::paint (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void ScrollViewWidget::paintOverChildren (Canvas& canvas)
{
    // Draw scrollbars on top
    if (showVScrollbar && contentHeight > getHeight())
        paintScrollbar (canvas, false);
    if (showHScrollbar && contentWidth > getWidth())
        paintScrollbar (canvas, true);
}

void ScrollViewWidget::resized()
{
    if (contentWidget)
    {
        contentWidget->setBounds (-scrollX, -scrollY, contentWidth, contentHeight);
    }
}

bool ScrollViewWidget::mouseWheel (const WheelEvent& e)
{
    float dx = e.deltaX * (e.isPixelDelta ? 1.0f : 40.0f);
    float dy = e.deltaY * (e.isPixelDelta ? 1.0f : 40.0f);

    setScrollOffset (scrollX - dx, scrollY - dy);
    return true;
}

void ScrollViewWidget::setContentWidget (Widget* content)
{
    if (contentWidget)
        removeChild (contentWidget);

    contentWidget = content;
    if (contentWidget)
    {
        addChild (contentWidget);
        contentWidget->setBounds (-scrollX, -scrollY, contentWidth, contentHeight);
    }
}

void ScrollViewWidget::setContentSize (float w, float h)
{
    contentWidth = w;
    contentHeight = h;
    clampScrollOffset();
    resized();
    repaint();
}

void ScrollViewWidget::setScrollOffset (float x, float y)
{
    scrollX = x;
    scrollY = y;
    clampScrollOffset();

    if (contentWidget)
        contentWidget->setBounds (-scrollX, -scrollY, contentWidth, contentHeight);

    repaint();
}

void ScrollViewWidget::scrollToMakeVisible (const Rect& area)
{
    float newX = scrollX;
    float newY = scrollY;

    if (area.x < scrollX)
        newX = area.x;
    else if (area.right() > scrollX + getWidth())
        newX = area.right() - getWidth();

    if (area.y < scrollY)
        newY = area.y;
    else if (area.bottom() > scrollY + getHeight())
        newY = area.bottom() - getHeight();

    setScrollOffset (newX, newY);
}

void ScrollViewWidget::clampScrollOffset()
{
    float maxX = std::max (0.0f, contentWidth - getWidth());
    float maxY = std::max (0.0f, contentHeight - getHeight());
    scrollX = std::clamp (scrollX, 0.0f, maxX);
    scrollY = std::clamp (scrollY, 0.0f, maxY);
}

void ScrollViewWidget::paintScrollbar (Canvas& canvas, bool horizontal)
{
    auto& theme = Theme::getDefault();
    float barSize = theme.scrollBarWidth;
    Color barColor = theme.outlineColor.withAlpha ((uint8_t) 128);

    if (horizontal)
    {
        float viewWidth = getWidth();
        float ratio = viewWidth / contentWidth;
        float thumbWidth = std::max (20.0f, viewWidth * ratio);
        float maxScroll = contentWidth - viewWidth;
        float thumbX = (maxScroll > 0) ? (scrollX / maxScroll) * (viewWidth - thumbWidth) : 0.0f;

        Rect bar (thumbX, getHeight() - barSize, thumbWidth, barSize);
        canvas.fillRoundedRect (bar, barSize * 0.5f, barColor);
    }
    else
    {
        float viewHeight = getHeight();
        float ratio = viewHeight / contentHeight;
        float thumbHeight = std::max (20.0f, viewHeight * ratio);
        float maxScroll = contentHeight - viewHeight;
        float thumbY = (maxScroll > 0) ? (scrollY / maxScroll) * (viewHeight - thumbHeight) : 0.0f;

        Rect bar (getWidth() - barSize, thumbY, barSize, thumbHeight);
        canvas.fillRoundedRect (bar, barSize * 0.5f, barColor);
    }
}

} // namespace gfx
} // namespace dc
