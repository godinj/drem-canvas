#pragma once

#include "graphics/core/Widget.h"
#include <functional>

namespace dc
{
namespace ui
{

class StepButtonWidget : public gfx::Widget
{
public:
    StepButtonWidget();

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;

    void setActive (bool active);
    bool isActive() const { return active; }

    void setVelocity (int vel);
    int getVelocity() const { return velocity; }

    void setPlaybackHighlight (bool hl);
    void setCursorHighlight (bool hl);
    void setBeatSeparator (bool sep);

    std::function<void()> onToggle;

private:
    bool active = false;
    int velocity = 100;
    bool playbackHighlighted = false;
    bool cursorHighlighted = false;
    bool beatSeparator = false;
};

} // namespace ui
} // namespace dc
