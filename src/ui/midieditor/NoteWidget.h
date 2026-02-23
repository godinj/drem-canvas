#pragma once

#include "graphics/core/Widget.h"
#include <functional>

namespace dc
{
namespace ui
{

class NoteWidget : public gfx::Widget
{
public:
    NoteWidget();

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;
    void mouseUp (const gfx::MouseEvent& e) override;

    void setNoteNumber (int note) { noteNumber = note; repaint(); }
    void setVelocity (int vel) { velocity = vel; repaint(); }
    void setSelected (bool sel) { selected = sel; repaint(); }

    int getNoteNumber() const { return noteNumber; }
    int getVelocity() const { return velocity; }
    bool isSelected() const { return selected; }

    std::function<void (float, float)> onDrag;  // dx, dy
    std::function<void (float)> onResize;        // new width

private:
    int noteNumber = 60;
    int velocity = 100;
    bool selected = false;
    bool dragging = false;
    bool resizing = false;
    float dragStartX = 0.0f;
    float dragStartY = 0.0f;
};

} // namespace ui
} // namespace dc
