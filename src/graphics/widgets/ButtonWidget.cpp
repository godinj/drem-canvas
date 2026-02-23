#include "ButtonWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace gfx
{

ButtonWidget::ButtonWidget (const std::string& t)
    : text (t)
{
}

void ButtonWidget::paint (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    Rect r (0, 0, getWidth(), getHeight());

    Color bg;
    if (toggled)
        bg = theme.buttonToggled;
    else if (pressed)
        bg = theme.buttonPressed;
    else if (hovered)
        bg = theme.buttonHover;
    else
        bg = theme.buttonDefault;

    canvas.fillRoundedRect (r, theme.buttonCornerRadius, bg);

    if (!text.empty())
    {
        Color textColor = toggled ? theme.brightText : theme.defaultText;
        canvas.drawTextCentred (text, r, FontManager::getInstance().getDefaultFont(), textColor);
    }
}

void ButtonWidget::mouseDown (const MouseEvent& e)
{
    pressed = true;
    repaint();
}

void ButtonWidget::mouseUp (const MouseEvent& e)
{
    if (pressed)
    {
        pressed = false;
        if (hovered)
        {
            if (toggleable)
                setToggleState (!toggled);
            if (onClick)
                onClick();
        }
        repaint();
    }
}

void ButtonWidget::mouseEnter (const MouseEvent& e)
{
    hovered = true;
    repaint();
}

void ButtonWidget::mouseExit (const MouseEvent& e)
{
    hovered = false;
    pressed = false;
    repaint();
}

void ButtonWidget::setText (const std::string& t)
{
    if (text != t)
    {
        text = t;
        repaint();
    }
}

void ButtonWidget::setToggleState (bool state)
{
    if (toggled != state)
    {
        toggled = state;
        repaint();
    }
}

} // namespace gfx
} // namespace dc
