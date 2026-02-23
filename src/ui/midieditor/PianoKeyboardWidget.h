#pragma once

#include "graphics/core/Widget.h"
#include <functional>

namespace dc
{
namespace ui
{

class PianoKeyboardWidget : public gfx::Widget
{
public:
    PianoKeyboardWidget();

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;

    void setScrollOffset (float offset) { scrollOffset = offset; repaint(); }
    float getRowHeight() const { return rowHeight; }

    std::function<void (int)> onNoteClicked; // MIDI note number

private:
    bool isBlackKey (int note) const;
    int noteFromY (float y) const;

    float rowHeight = 12.0f;
    float scrollOffset = 0.0f;
};

} // namespace ui
} // namespace dc
