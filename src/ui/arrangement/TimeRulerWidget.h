#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include "model/TempoMap.h"
#include "engine/TransportController.h"
#include <functional>

namespace dc
{
namespace ui
{

class TimeRulerWidget : public gfx::Widget
{
public:
    TimeRulerWidget (const TempoMap& tempoMap, const TransportController& transport);

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

    const TempoMap& tempoMap;
    const TransportController& transportController;
    double pixelsPerSecond = 100.0;
    double scrollOffset = 0.0;
    static constexpr float headerWidth = 150.0f;
};

} // namespace ui
} // namespace dc
