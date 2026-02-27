#pragma once

#include "graphics/core/Widget.h"
#include "vim/VirtualKeyboardState.h"

namespace dc
{
namespace ui
{

class VirtualKeyboardWidget : public gfx::Widget,
                              public VirtualKeyboardState::Listener
{
public:
    static constexpr float preferredHeight = 80.0f;

    explicit VirtualKeyboardWidget (VirtualKeyboardState& state);
    ~VirtualKeyboardWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void animationTick (double timestampMs) override;

    // VirtualKeyboardState::Listener
    void keyboardStateChanged() override;

private:
    VirtualKeyboardState& kbState;
};

} // namespace ui
} // namespace dc
