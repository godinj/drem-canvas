#include "PluginSlotListWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace ui
{

PluginSlotListWidget::PluginSlotListWidget()
{
}

void PluginSlotListWidget::setSelectedSlotIndex (int index)
{
    if (selectedSlotIndex != index)
    {
        selectedSlotIndex = index;
        repaint();
    }
}

void PluginSlotListWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getSmallFont();
    float w = getWidth();

    // Draw populated slots
    for (size_t i = 0; i < slots.size(); ++i)
    {
        float y = static_cast<float> (i) * slotHeight;
        Rect slotRect (0, y, w, slotHeight);

        Color bg = slots[i].bypassed
            ? Color::fromARGB (0xff3a2a2a)
            : theme.widgetBackground;

        canvas.fillRect (slotRect, bg);

        // Selected slot highlight
        if (static_cast<int> (i) == selectedSlotIndex)
        {
            canvas.fillRect (slotRect, theme.selection.withAlpha ((uint8_t) 38));
            canvas.strokeRect (slotRect, theme.selection, 1.0f);
        }
        else
        {
            canvas.strokeRect (slotRect, theme.outlineColor, 0.5f);
        }

        if (!slots[i].name.empty())
        {
            Color textColor = slots[i].bypassed ? theme.dimText : theme.defaultText;
            canvas.drawText (slots[i].name, 4.0f, y + slotHeight * 0.5f + 4.0f,
                             font, textColor);
        }
        else
        {
            canvas.drawText ("(empty)", 4.0f, y + slotHeight * 0.5f + 4.0f,
                             font, theme.dimText);
        }
    }

    // Draw "add" slot if selected past the last plugin
    if (selectedSlotIndex >= 0 && selectedSlotIndex >= static_cast<int> (slots.size()))
    {
        float y = static_cast<float> (slots.size()) * slotHeight;
        Rect slotRect (0, y, w, slotHeight);
        canvas.fillRect (slotRect, theme.selection.withAlpha ((uint8_t) 38));
        canvas.strokeRect (slotRect, theme.selection, 1.0f);
        canvas.drawText ("[+]", 4.0f, y + slotHeight * 0.5f + 4.0f, font, theme.selection);
    }
}

void PluginSlotListWidget::mouseDown (const gfx::MouseEvent& e)
{
    int index = static_cast<int> (e.y / slotHeight);
    if (index < 0 || index >= static_cast<int> (slots.size()))
        return;

    if (e.rightButton)
    {
        if (onSlotRightClicked)
            onSlotRightClicked (index);
    }
    else
    {
        if (onSlotClicked)
            onSlotClicked (index);
    }
}

void PluginSlotListWidget::setSlots (const std::vector<PluginSlot>& newSlots)
{
    slots = newSlots;
    repaint();
}

} // namespace ui
} // namespace dc
