#include "CpuMeterWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include <algorithm>
#include <cstdio>

namespace dc
{
namespace ui
{

CpuMeterWidget::CpuMeterWidget()
{
    setAnimating (true);
}

void CpuMeterWidget::setCpuLoad (float load)
{
    rawLoad = std::max (0.0f, std::min (load, 1.0f));
}

void CpuMeterWidget::animationTick (double timestampMs)
{
    // Exponential smoothing: fast rise, slow fall
    float alpha = (rawLoad > smoothedLoad) ? 0.3f : 0.05f;
    smoothedLoad += alpha * (rawLoad - smoothedLoad);

    // Peak hold (3 second hold, then decay)
    if (smoothedLoad > peakHold)
    {
        peakHold = smoothedLoad;
        peakHoldTimer = timestampMs;
    }
    else if (timestampMs - peakHoldTimer > 3000.0)
    {
        peakHold *= 0.95f;
    }

    repaint();
}

void CpuMeterWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRoundedRect (Rect (0, 0, w, h), 3.0f, theme.meterBackground);

    float margin = 2.0f;
    float labelW = 26.0f;
    float textW = 36.0f;
    float barX = labelW;
    float barW = w - labelW - textW;
    float barH = h - 2.0f * margin;
    float barY = margin;

    // "CPU" label
    auto labelFont = FontManager::getInstance().makeFont (10.0f);
    canvas.drawText ("CPU", margin + 1.0f, h * 0.5f + 4.0f, labelFont, theme.brightText);

    // Bar fill — minimum 3px so the bar is always visible when engine is active
    if (barW > 0 && smoothedLoad > 0.0001f)
    {
        float fillW = std::max (3.0f, smoothedLoad * barW);

        Color barColor = theme.meterGreen;
        if (smoothedLoad > 0.85f)
            barColor = theme.meterRed;
        else if (smoothedLoad > 0.6f)
            barColor = theme.meterYellow;

        canvas.fillRoundedRect (Rect (barX, barY, fillW, barH), 2.0f, barColor);
    }

    // Peak hold line
    if (peakHold > 0.005f && barW > 0)
    {
        float peakX = barX + std::max (3.0f, peakHold * barW);
        canvas.drawLine (peakX, barY, peakX, barY + barH, theme.brightText, 1.0f);
    }

    // Percentage text
    int pct = static_cast<int> (smoothedLoad * 100.0f + 0.5f);
    if (pct > 100) pct = 100;
    char buf[8];
    if (pct == 0 && smoothedLoad > 0.0001f)
        std::snprintf (buf, sizeof (buf), "<1%%");
    else
        std::snprintf (buf, sizeof (buf), "%d%%", pct);

    auto monoFont = FontManager::getInstance().makeMonoFont (10.0f);
    canvas.drawTextRight (std::string (buf),
                          Rect (w - textW, 0, textW - margin, h),
                          monoFont, theme.brightText);
}

} // namespace ui
} // namespace dc
