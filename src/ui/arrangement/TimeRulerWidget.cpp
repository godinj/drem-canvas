#include "TimeRulerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"
#include <cmath>
#include <string>

namespace dc
{
namespace ui
{

TimeRulerWidget::TimeRulerWidget (const TempoMap& tempo)
    : tempoMap (tempo)
{
}

void TimeRulerWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getSmallFont();

    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff252535));

    double bpm = tempoMap.getTempo();
    int beatsPerBar = tempoMap.getTimeSigNumerator();
    double secondsPerBeat = 60.0 / bpm;
    double secondsPerBar = secondsPerBeat * beatsPerBar;
    double pixelsPerBar = secondsPerBar * pixelsPerSecond;

    // Determine bar interval based on zoom level (skip bars when zoomed out)
    int barInterval = 1;
    if (pixelsPerBar < 20.0) barInterval = 16;
    else if (pixelsPerBar < 40.0) barInterval = 8;
    else if (pixelsPerBar < 80.0) barInterval = 4;
    else if (pixelsPerBar < 160.0) barInterval = 2;

    double startTime = scrollOffset / pixelsPerSecond;
    int startBar = std::max (1, static_cast<int> (std::floor (startTime / secondsPerBar)) + 1);

    // Align to bar interval
    startBar = ((startBar - 1) / barInterval) * barInterval + 1;

    for (int bar = startBar; ; bar += barInterval)
    {
        double barTime = (bar - 1) * secondsPerBar;
        float x = static_cast<float> ((barTime - startTime) * pixelsPerSecond) + headerWidth;
        if (x > w) break;

        if (x >= headerWidth)
        {
            // Major tick at bar line
            canvas.drawLine (x, 0, x, h, Color::fromARGB (0xff555565), 1.0f);

            // Bar number label
            std::string label = std::to_string (bar);
            canvas.drawText (label, x + 3.0f, h - 4.0f, font, theme.dimText);
        }

        // Draw beat ticks within the bar (only when zoomed in enough)
        if (barInterval == 1 && pixelsPerBar >= 80.0)
        {
            for (int beat = 1; beat < beatsPerBar; ++beat)
            {
                double beatTime = barTime + beat * secondsPerBeat;
                float bx = static_cast<float> ((beatTime - startTime) * pixelsPerSecond) + headerWidth;
                if (bx >= headerWidth && bx <= w)
                    canvas.drawLine (bx, h * 0.5f, bx, h, Color::fromARGB (0xff404050), 1.0f);
            }
        }
    }

    // Bottom border
    canvas.drawLine (0, h - 1.0f, w, h - 1.0f, theme.outlineColor, 1.0f);
}

void TimeRulerWidget::mouseDown (const gfx::MouseEvent& e)
{
    seekFromX (e.x);
}

void TimeRulerWidget::mouseDrag (const gfx::MouseEvent& e)
{
    seekFromX (e.x);
}

void TimeRulerWidget::seekFromX (float mouseX)
{
    double timeInSeconds = (static_cast<double> (mouseX - headerWidth) + scrollOffset) / pixelsPerSecond;
    if (timeInSeconds >= 0.0 && onSeek)
        onSeek (timeInSeconds);
}

} // namespace ui
} // namespace dc
