#pragma once

#include "graphics/core/Widget.h"
#include "engine/TransportController.h"
#include "model/Project.h"
#include <functional>

namespace dc
{
namespace ui
{

class PianoRollRulerWidget : public gfx::Widget
{
public:
    PianoRollRulerWidget (const TransportController& transport, const Project& project);

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;

    void setPixelsPerBeat (float ppb) { pixelsPerBeat = ppb; repaint(); }
    void setScrollOffset (float offset) { scrollOffset = offset; repaint(); }
    void setTimeSigNumerator (int num) { timeSigNumerator = num; repaint(); }
    void setBeatOffset (double offset) { beatOffset = offset; repaint(); }

    std::function<void (double)> onSeek; // beat position

    static constexpr float rulerHeight = 24.0f;

private:
    const TransportController& transportController;
    const Project& project;
    float pixelsPerBeat = 80.0f;
    float scrollOffset = 0.0f;
    int timeSigNumerator = 4;
    double beatOffset = 0.0;  // clip start position in beats
};

} // namespace ui
} // namespace dc
