#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <functional>
#include <string>

namespace dc
{
namespace gfx
{

class ButtonWidget : public Widget
{
public:
    explicit ButtonWidget (const std::string& text = "");

    void paint (Canvas& canvas) override;

    void mouseDown (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;

    void setText (const std::string& text);
    const std::string& getText() const { return text; }

    void setToggleable (bool t) { toggleable = t; }
    bool isToggleable() const { return toggleable; }

    void setToggleState (bool state);
    bool getToggleState() const { return toggled; }

    std::function<void()> onClick;

private:
    std::string text;
    bool hovered = false;
    bool pressed = false;
    bool toggleable = false;
    bool toggled = false;
};

} // namespace gfx
} // namespace dc
