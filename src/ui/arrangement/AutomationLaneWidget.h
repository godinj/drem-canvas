#pragma once

#include "graphics/core/Widget.h"
#include <JuceHeader.h>
#include <vector>

namespace dc
{
namespace ui
{

class AutomationLaneWidget : public gfx::Widget
{
public:
    AutomationLaneWidget();

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;

    struct BreakPoint
    {
        double timeSamples = 0.0;
        float value = 0.0f; // 0.0 to 1.0
    };

    void setBreakpoints (const std::vector<BreakPoint>& pts);
    void setPixelsPerSecond (double pps) { pixelsPerSecond = pps; repaint(); }
    void setSampleRate (double sr) { sampleRate = sr; repaint(); }

private:
    std::vector<BreakPoint> breakpoints;
    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;
    int dragPointIndex = -1;
};

} // namespace ui
} // namespace dc
