#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <string>

namespace dc
{
namespace gfx
{

class LabelWidget : public Widget
{
public:
    enum Alignment { Left, Centre, Right };

    explicit LabelWidget (const std::string& text = "", Alignment align = Left);

    void paint (Canvas& canvas) override;

    void setText (const std::string& text);
    const std::string& getText() const { return text; }

    void setAlignment (Alignment a) { alignment = a; repaint(); }
    Alignment getAlignment() const { return alignment; }

    void setTextColor (Color c) { textColor = c; repaint(); }
    void setFontSize (float size);
    void setUseMonoFont (bool mono) { useMono = mono; repaint(); }

private:
    std::string text;
    Alignment alignment = Left;
    Color textColor;
    float fontSize = 0.0f; // 0 = use default
    bool useMono = false;
};

} // namespace gfx
} // namespace dc
