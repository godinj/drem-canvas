#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <functional>

namespace dc
{
namespace ui
{

class TimeRulerWidget : public gfx::Widget
{
public:
    TimeRulerWidget();

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;

    void setPixelsPerSecond (double pps) { pixelsPerSecond = pps; repaint(); }
    void setScrollOffset (double offset) { scrollOffset = offset; repaint(); }

    double getPixelsPerSecond() const { return pixelsPerSecond; }
    double getScrollOffset() const { return scrollOffset; }

    std::function<void (double)> onSeek;

private:
    void seekFromX (float mouseX);

    double pixelsPerSecond = 100.0;
    double scrollOffset = 0.0;
    static constexpr float headerWidth = 150.0f;
};

} // namespace ui
} // namespace dc
