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

    // Draw slots (populated + empty)
    int totalSlots = std::max (static_cast<int> (slots.size()), 4);
    // Show one extra if selected past the end (the "add" slot)
    if (selectedSlotIndex >= totalSlots)
        totalSlots = selectedSlotIndex + 1;

    for (int i = 0; i < totalSlots; ++i)
    {
        float y = static_cast<float> (i) * slotHeight;
        Rect slotRect (0, y, w, slotHeight);

        Color bg = (i < static_cast<int> (slots.size()) && slots[i].bypassed)
            ? Color::fromARGB (0xff3a2a2a)
            : theme.widgetBackground;

        canvas.fillRect (slotRect, bg);

        // Selected slot highlight — bright cursor bar + distinct background
        if (i == selectedSlotIndex)
        {
            canvas.fillRect (slotRect, theme.selection.withAlpha ((uint8_t) 90));

            // Solid green cursor bar on left edge
            Rect cursorBar (0, slotRect.y, 3.0f, slotRect.height);
            canvas.fillRect (cursorBar, theme.selection);
        }
        else
        {
            canvas.strokeRect (slotRect, theme.outlineColor, 0.5f);
        }

        // Slot number prefix
        std::string prefix = std::to_string (i + 1) + ": ";

        if (i < static_cast<int> (slots.size()) && !slots[i].name.empty())
        {
            Color textColor = slots[i].bypassed ? theme.dimText : theme.defaultText;
            canvas.drawText (prefix + slots[i].name, 4.0f, y + slotHeight * 0.5f + 4.0f,
                             font, textColor);
        }
        else if (i == selectedSlotIndex && i >= static_cast<int> (slots.size()))
        {
            // "Add" slot
            canvas.drawText (prefix + "[+]", 4.0f, y + slotHeight * 0.5f + 4.0f,
                             font, theme.selection);
        }
        else
        {
            // Empty slot — show number
            canvas.drawText (prefix, 4.0f, y + slotHeight * 0.5f + 4.0f,
                             font, theme.dimText.withAlpha ((uint8_t) 100));
        }
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
