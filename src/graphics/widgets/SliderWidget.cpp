#include "SliderWidget.h"
#include "graphics/rendering/Canvas.h"
#include "include/core/SkPath.h"
#include <cmath>
#include <algorithm>

namespace dc
{
namespace gfx
{

SliderWidget::SliderWidget (Style s)
    : style (s)
{
}

void SliderWidget::paint (Canvas& canvas)
{
    switch (style)
    {
        case LinearVertical:    paintLinearVertical (canvas); break;
        case LinearHorizontal:  paintLinearHorizontal (canvas); break;
        case Rotary:            paintRotary (canvas); break;
    }
}

void SliderWidget::paintLinearVertical (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();
    float centreX = w * 0.5f;
    float trackWidth = 4.0f;

    // Track
    canvas.fillRoundedRect (Rect (centreX - trackWidth * 0.5f, 0, trackWidth, h),
                            2.0f, theme.sliderTrack);

    // Thumb
    float proportion = static_cast<float> ((value - minValue) / (maxValue - minValue));
    float thumbY = h - proportion * h; // Inverted: bottom = min
    float thumbWidth = 20.0f;
    float thumbHeight = 10.0f;

    canvas.fillRoundedRect (
        Rect (centreX - thumbWidth * 0.5f, thumbY - thumbHeight * 0.5f, thumbWidth, thumbHeight),
        3.0f, theme.sliderThumb);
}

void SliderWidget::paintLinearHorizontal (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();
    float centreY = h * 0.5f;
    float trackHeight = 4.0f;

    // Track
    canvas.fillRoundedRect (Rect (0, centreY - trackHeight * 0.5f, w, trackHeight),
                            2.0f, theme.sliderTrack);

    // Thumb
    float proportion = static_cast<float> ((value - minValue) / (maxValue - minValue));
    float thumbX = proportion * w;
    float thumbWidth = 10.0f;
    float thumbHeight = 20.0f;

    canvas.fillRoundedRect (
        Rect (thumbX - thumbWidth * 0.5f, centreY - thumbHeight * 0.5f, thumbWidth, thumbHeight),
        3.0f, theme.sliderThumb);
}

void SliderWidget::paintRotary (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();
    float radius = std::min (w, h) * 0.5f - 4.0f;
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    constexpr float pi = 3.14159265358979f;
    float startAngle = pi * 0.75f;
    float endAngle = pi * 2.25f;
    float proportion = static_cast<float> ((value - minValue) / (maxValue - minValue));
    float angle = startAngle + proportion * (endAngle - startAngle);

    // Background circle
    canvas.fillCircle (cx, cy, radius, theme.outlineColor);

    // Filled arc from start to current position
    float arcThickness = 3.0f;
    float arcRadius = radius - arcThickness * 0.5f;

    SkPath arcPath;
    arcPath.addArc (
        SkRect::MakeXYWH (cx - arcRadius, cy - arcRadius, arcRadius * 2.0f, arcRadius * 2.0f),
        (startAngle - pi * 0.5f) * (180.0f / pi),
        (angle - startAngle) * (180.0f / pi));

    canvas.strokePath (arcPath, theme.accent, arcThickness);

    // Dot indicator
    float dotRadius = 3.0f;
    float dotDistance = arcRadius;
    float dotX = cx + dotDistance * std::cos (angle - pi * 0.5f);
    float dotY = cy + dotDistance * std::sin (angle - pi * 0.5f);
    canvas.fillCircle (dotX, dotY, dotRadius, theme.brightText);
}

void SliderWidget::mouseDown (const MouseEvent& e)
{
    dragging = true;
    dragStartY = e.y;
    dragStartX = e.x;
    dragStartValue = value;
}

void SliderWidget::mouseDrag (const MouseEvent& e)
{
    if (!dragging)
        return;

    double newValue;
    if (style == LinearVertical)
    {
        float delta = dragStartY - e.y;
        double sensitivity = (maxValue - minValue) / static_cast<double> (getHeight());
        newValue = dragStartValue + static_cast<double> (delta) * sensitivity;
    }
    else if (style == LinearHorizontal)
    {
        float delta = e.x - dragStartX;
        double sensitivity = (maxValue - minValue) / static_cast<double> (getWidth());
        newValue = dragStartValue + static_cast<double> (delta) * sensitivity;
    }
    else // Rotary
    {
        float delta = dragStartY - e.y;
        double sensitivity = (maxValue - minValue) / 200.0;
        newValue = dragStartValue + static_cast<double> (delta) * sensitivity;
    }

    setValue (newValue);
}

void SliderWidget::mouseUp (const MouseEvent& e)
{
    dragging = false;
}

void SliderWidget::setValue (double newValue)
{
    newValue = std::clamp (newValue, minValue, maxValue);
    if (value != newValue)
    {
        value = newValue;
        repaint();
        if (onValueChange)
            onValueChange (value);
    }
}

void SliderWidget::setRange (double min, double max)
{
    minValue = min;
    maxValue = max;
    setValue (std::clamp (value, minValue, maxValue));
}

} // namespace gfx
} // namespace dc
