#include "TimeRulerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"
#include <cmath>
#include <string>

namespace dc
{
namespace ui
{

TimeRulerWidget::TimeRulerWidget()
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

    double startTime = scrollOffset / pixelsPerSecond;

    // Determine tick interval based on zoom level
    double interval;
    if (pixelsPerSecond >= 200.0) interval = 1.0;
    else if (pixelsPerSecond >= 50.0) interval = 5.0;
    else if (pixelsPerSecond >= 20.0) interval = 10.0;
    else if (pixelsPerSecond >= 5.0) interval = 30.0;
    else interval = 60.0;

    double firstTick = std::floor (startTime / interval) * interval;

    for (double t = firstTick; ; t += interval)
    {
        float x = static_cast<float> ((t - startTime) * pixelsPerSecond) + headerWidth;
        if (x > w) break;
        if (x < headerWidth) continue;

        // Major tick
        canvas.drawLine (x, 0, x, h, Color::fromARGB (0xff555565), 1.0f);

        // Time label
        int totalSeconds = static_cast<int> (t);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        char buf[16];
        snprintf (buf, sizeof (buf), "%d:%02d", minutes, seconds);
        canvas.drawText (std::string (buf), x + 3.0f, h - 4.0f, font, theme.dimText);

        // Minor ticks
        if (interval <= 10.0)
        {
            int numMinor = (interval <= 1.0) ? 4 : 5;
            double minorInterval = interval / numMinor;
            for (int m = 1; m < numMinor; ++m)
            {
                float mx = static_cast<float> ((t + m * minorInterval - startTime) * pixelsPerSecond) + headerWidth;
                if (mx >= headerWidth && mx <= w)
                    canvas.drawLine (mx, h * 0.5f, mx, h, Color::fromARGB (0xff404050), 1.0f);
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
