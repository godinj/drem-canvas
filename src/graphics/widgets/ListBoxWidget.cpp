#include "ListBoxWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"
#include <algorithm>

namespace dc
{
namespace gfx
{

ListBoxWidget::ListBoxWidget()
{
}

void ListBoxWidget::paint (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);

    int start = getVisibleRowStart();
    int count = getVisibleRowCount();

    for (int i = start; i < start + count && i < static_cast<int> (items.size()); ++i)
    {
        float y = static_cast<float> (i) * rowHeight - scrollOffset;
        Rect rowRect (0, y, getWidth(), rowHeight);

        bool isSelected = (i == selectedIndex);

        if (isSelected)
            canvas.fillRect (rowRect, theme.selection.withAlpha ((uint8_t) 80));

        if (customRowPaint)
        {
            customRowPaint (canvas, i, rowRect, isSelected);
        }
        else
        {
            Color textColor = isSelected ? theme.brightText : theme.defaultText;
            canvas.drawText (items[static_cast<size_t> (i)],
                             8.0f, y + rowHeight * 0.5f + 4.0f,
                             FontManager::getInstance().getDefaultFont(),
                             textColor);
        }

        // Separator line
        canvas.drawLine (0, y + rowHeight, getWidth(), y + rowHeight,
                         theme.outlineColor.withAlpha ((uint8_t) 60));
    }
}

void ListBoxWidget::mouseDown (const MouseEvent& e)
{
    int index = static_cast<int> ((e.y + scrollOffset) / rowHeight);
    if (index >= 0 && index < static_cast<int> (items.size()))
        setSelectedIndex (index);
}

void ListBoxWidget::mouseDoubleClick (const MouseEvent& e)
{
    int index = static_cast<int> ((e.y + scrollOffset) / rowHeight);
    if (index >= 0 && index < static_cast<int> (items.size()))
    {
        setSelectedIndex (index);

        if (onDoubleClick)
            onDoubleClick (index);
    }
}

bool ListBoxWidget::mouseWheel (const WheelEvent& e)
{
    float delta = e.deltaY * (e.isPixelDelta ? 1.0f : 40.0f);
    float maxScroll = std::max (0.0f, static_cast<float> (items.size()) * rowHeight - getHeight());
    scrollOffset = std::clamp (scrollOffset - delta, 0.0f, maxScroll);
    repaint();
    return true;
}

void ListBoxWidget::setItems (const std::vector<std::string>& newItems)
{
    items = newItems;
    scrollOffset = 0.0f;
    selectedIndex = -1;
    repaint();
}

void ListBoxWidget::setSelectedIndex (int index)
{
    if (index != selectedIndex)
    {
        selectedIndex = index;
        scrollToEnsureIndexVisible (index);
        repaint();
        if (onSelectionChanged)
            onSelectionChanged (index);
    }
}

void ListBoxWidget::scrollToEnsureIndexVisible (int index)
{
    if (index < 0 || index >= static_cast<int> (items.size()))
        return;

    float rowTop = index * rowHeight;
    float rowBottom = rowTop + rowHeight;
    float maxScroll = std::max (0.0f, static_cast<float> (items.size()) * rowHeight - getHeight());

    if (rowTop < scrollOffset)
        scrollOffset = rowTop;
    else if (rowBottom > scrollOffset + getHeight())
        scrollOffset = rowBottom - getHeight();

    scrollOffset = std::clamp (scrollOffset, 0.0f, maxScroll);
}

int ListBoxWidget::getVisibleRowStart() const
{
    return std::max (0, static_cast<int> (scrollOffset / rowHeight));
}

int ListBoxWidget::getVisibleRowCount() const
{
    return static_cast<int> (getHeight() / rowHeight) + 2;
}

} // namespace gfx
} // namespace dc
