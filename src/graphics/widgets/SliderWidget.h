#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <functional>

namespace dc
{
namespace gfx
{

class SliderWidget : public Widget
{
public:
    enum Style { LinearVertical, LinearHorizontal, Rotary };

    explicit SliderWidget (Style style = LinearVertical);

    void paint (Canvas& canvas) override;

    void mouseDown (const MouseEvent& e) override;
    void mouseDrag (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;

    void setValue (double newValue);
    double getValue() const { return value; }

    void setRange (double min, double max);
    double getMinimum() const { return minValue; }
    double getMaximum() const { return maxValue; }

    void setStyle (Style s) { style = s; repaint(); }
    Style getStyle() const { return style; }

    std::function<void (double)> onValueChange;

private:
    void paintLinearVertical (Canvas& canvas);
    void paintLinearHorizontal (Canvas& canvas);
    void paintRotary (Canvas& canvas);

    double valueToPosition (double v) const;
    double positionToValue (float pos) const;

    Style style;
    double value = 0.5;
    double minValue = 0.0;
    double maxValue = 1.0;
    float dragStartY = 0.0f;
    float dragStartX = 0.0f;
    double dragStartValue = 0.0;
    bool dragging = false;
};

} // namespace gfx
} // namespace dc
