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

void PluginSlotListWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getSmallFont();
    float w = getWidth();

    for (size_t i = 0; i < slots.size(); ++i)
    {
        float y = static_cast<float> (i) * slotHeight;
        Rect slotRect (0, y, w, slotHeight);

        Color bg = slots[i].bypassed
            ? Color::fromARGB (0xff3a2a2a)
            : theme.widgetBackground;

        canvas.fillRect (slotRect, bg);
        canvas.strokeRect (slotRect, theme.outlineColor, 0.5f);

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
