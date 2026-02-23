#include "AutomationLaneWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "include/core/SkPath.h"

namespace dc
{
namespace ui
{

AutomationLaneWidget::AutomationLaneWidget()
{
}

void AutomationLaneWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff1a1a2a).withAlpha ((uint8_t) 128));

    if (breakpoints.empty())
        return;

    // Draw automation path
    SkPath path;
    bool first = true;

    for (const auto& bp : breakpoints)
    {
        float x = static_cast<float> ((bp.timeSamples / sampleRate) * pixelsPerSecond);
        float y = h - bp.value * h;

        if (first)
        {
            path.moveTo (x, y);
            first = false;
        }
        else
        {
            path.lineTo (x, y);
        }
    }

    canvas.strokePath (path, theme.accent, 2.0f);

    // Draw breakpoint dots
    for (const auto& bp : breakpoints)
    {
        float x = static_cast<float> ((bp.timeSamples / sampleRate) * pixelsPerSecond);
        float y = h - bp.value * h;
        canvas.fillCircle (x, y, 4.0f, theme.accent);
        canvas.strokeCircle (x, y, 4.0f, theme.brightText, 1.0f);
    }
}

void AutomationLaneWidget::mouseDown (const gfx::MouseEvent& e)
{
    // Find nearest breakpoint
    dragPointIndex = -1;
    float minDist = 10.0f;
    float h = getHeight();

    for (int i = 0; i < static_cast<int> (breakpoints.size()); ++i)
    {
        float bpX = static_cast<float> ((breakpoints[static_cast<size_t> (i)].timeSamples / sampleRate) * pixelsPerSecond);
        float bpY = h - breakpoints[static_cast<size_t> (i)].value * h;
        float dx = e.x - bpX;
        float dy = e.y - bpY;
        float dist = std::sqrt (dx * dx + dy * dy);
        if (dist < minDist)
        {
            minDist = dist;
            dragPointIndex = i;
        }
    }
}

void AutomationLaneWidget::mouseDrag (const gfx::MouseEvent& e)
{
    if (dragPointIndex >= 0 && dragPointIndex < static_cast<int> (breakpoints.size()))
    {
        float h = getHeight();
        float value = 1.0f - (e.y / h);
        value = std::clamp (value, 0.0f, 1.0f);
        breakpoints[static_cast<size_t> (dragPointIndex)].value = value;
        repaint();
    }
}

void AutomationLaneWidget::setBreakpoints (const std::vector<BreakPoint>& pts)
{
    breakpoints = pts;
    repaint();
}

} // namespace ui
} // namespace dc
