#include "ProgressBarWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"
#include <algorithm>
#include <cstdio>

namespace dc
{
namespace gfx
{

ProgressBarWidget::ProgressBarWidget()
{
}

void ProgressBarWidget::paint (Canvas& canvas)
{
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();
    float radius = 4.0f;

    // Track (background)
    canvas.fillRoundedRect (Rect (0, 0, w, h), radius, theme.sliderTrack);

    // Fill (foreground) — only draw when progress > 0
    if (progress_ > 0.0)
    {
        float fillWidth = static_cast<float> (w * progress_);
        canvas.fillRoundedRect (Rect (0, 0, fillWidth, h), radius, theme.accent);
    }

    auto& fm = FontManager::getInstance();
    const SkFont& font = fm.getDefaultFont();

    // Status text — left-aligned with 8px left padding
    if (!statusText_.empty())
    {
        float textY = h * 0.5f + font.getSize() * 0.35f;
        canvas.drawText (statusText_, 8.0f, textY, font, theme.defaultText);
    }

    // Percentage text — right-aligned with 4px padding
    char percentBuf[8];
    std::snprintf (percentBuf, sizeof (percentBuf), "%d%%",
                   static_cast<int> (progress_ * 100.0));
    Rect textRect (0, 0, w - 4.0f, h);
    canvas.drawTextRight (std::string (percentBuf), textRect, font, theme.brightText);
}

void ProgressBarWidget::setProgress (double progress)
{
    progress = std::clamp (progress, 0.0, 1.0);
    if (progress_ != progress)
    {
        progress_ = progress;
        repaint();
    }
}

void ProgressBarWidget::setStatusText (const std::string& text)
{
    if (statusText_ != text)
    {
        statusText_ = text;
        repaint();
    }
}

} // namespace gfx
} // namespace dc
