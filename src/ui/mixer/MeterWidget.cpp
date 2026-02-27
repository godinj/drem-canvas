#include "MeterWidget.h"
#include "graphics/rendering/Canvas.h"
#include <cmath>
#include <algorithm>

namespace dc
{
namespace ui
{

MeterWidget::MeterWidget()
{
    setAnimating (true);
}

void MeterWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRect (Rect (0, 0, w, h), theme.meterBackground);

    float halfW = w * 0.5f - 1.0f;

    // Draw meter bars (gradient: green → yellow → red)
    auto drawBar = [&] (float x, float barW, float level)
    {
        float barH = level * h;
        if (barH <= 0) return;

        float greenEnd = h * 0.6f;
        float yellowEnd = h * 0.85f;

        // Green portion
        float greenH = std::min (barH, greenEnd);
        if (greenH > 0)
            canvas.fillRect (Rect (x, h - greenH, barW, greenH), theme.meterGreen);

        // Yellow portion
        if (barH > greenEnd)
        {
            float yellowH = std::min (barH - greenEnd, yellowEnd - greenEnd);
            canvas.fillRect (Rect (x, h - greenEnd - yellowH, barW, yellowH), theme.meterYellow);
        }

        // Red portion
        if (barH > yellowEnd)
        {
            float redH = barH - yellowEnd;
            canvas.fillRect (Rect (x, h - yellowEnd - redH, barW, redH), theme.meterRed);
        }
    };

    drawBar (0, halfW, displayLeft);
    drawBar (halfW + 2.0f, halfW, displayRight);

    // Peak hold indicators
    if (peakHoldLeft > 0.01f)
    {
        float y = h - peakHoldLeft * h;
        canvas.drawLine (0, y, halfW, y, theme.brightText, 1.0f);
    }
    if (peakHoldRight > 0.01f)
    {
        float y = h - peakHoldRight * h;
        canvas.drawLine (halfW + 2.0f, y, w, y, theme.brightText, 1.0f);
    }

    // Center separator
    canvas.drawLine (halfW + 1.0f, 0, halfW + 1.0f, h, theme.outlineColor, 1.0f);
}

void MeterWidget::animationTick (double timestampMs)
{
    // Convert dB to linear (0-1 range, where 0dB = 1.0)
    auto dbToLinear = [] (float db) -> float
    {
        if (db <= -60.0f) return 0.0f;
        return std::pow (10.0f, db / 20.0f);
    };

    float targetLeft = dbToLinear (leftLevel);
    float targetRight = dbToLinear (rightLevel);

    // Smooth fall-off
    float decay = 0.92f;
    displayLeft = std::max (targetLeft, displayLeft * decay);
    displayRight = std::max (targetRight, displayRight * decay);

    // Peak hold
    if (targetLeft > peakHoldLeft)
    {
        peakHoldLeft = targetLeft;
        peakHoldTimerLeft = timestampMs;
    }
    else if (timestampMs - peakHoldTimerLeft > 2000.0) // 2 second hold
    {
        peakHoldLeft *= 0.95f;
    }

    if (targetRight > peakHoldRight)
    {
        peakHoldRight = targetRight;
        peakHoldTimerRight = timestampMs;
    }
    else if (timestampMs - peakHoldTimerRight > 2000.0)
    {
        peakHoldRight *= 0.95f;
    }

    repaint();
}

void MeterWidget::setLevel (float leftDb, float rightDb)
{
    leftLevel = leftDb;
    rightLevel = rightDb;

    // Convert dB to linear for direct display update
    auto dbToLinear = [] (float db) -> float
    {
        if (db <= -60.0f) return 0.0f;
        return std::pow (10.0f, db / 20.0f);
    };

    float targetLeft = dbToLinear (leftDb);
    float targetRight = dbToLinear (rightDb);

    // Smooth fall-off
    float decay = 0.92f;
    displayLeft = std::max (targetLeft, displayLeft * decay);
    displayRight = std::max (targetRight, displayRight * decay);

    // Peak hold (use simple monotonic clock)
    double now = juce::Time::getMillisecondCounterHiRes();
    if (targetLeft > peakHoldLeft)
    {
        peakHoldLeft = targetLeft;
        peakHoldTimerLeft = now;
    }
    else if (now - peakHoldTimerLeft > 2000.0)
    {
        peakHoldLeft *= 0.95f;
    }

    if (targetRight > peakHoldRight)
    {
        peakHoldRight = targetRight;
        peakHoldTimerRight = now;
    }
    else if (now - peakHoldTimerRight > 2000.0)
    {
        peakHoldRight *= 0.95f;
    }

    repaint();
}

void MeterWidget::setPeakHold (float lp, float rp)
{
    leftPeak = lp;
    rightPeak = rp;
}

} // namespace ui
} // namespace dc
