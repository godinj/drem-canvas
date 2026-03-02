#pragma once

#include "graphics/core/Widget.h"
#include "model/Project.h"
#include "model/MidiClip.h"
#include <set>

namespace dc
{
namespace ui
{

class VelocityLaneWidget : public gfx::Widget
{
public:
    VelocityLaneWidget (Project& project);

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;
    void mouseUp (const gfx::MouseEvent& e) override;

    void setClipState (const PropertyTree& state) { clipState = state; repaint(); }
    void setPixelsPerBeat (float ppb) { pixelsPerBeat = ppb; repaint(); }
    void setScrollOffset (float offset) { scrollOffset = offset; repaint(); }
    void setSelectedNotes (const std::set<int>* sel) { selectedNotes = sel; repaint(); }

private:
    Project& project;
    PropertyTree clipState;
    float pixelsPerBeat = 80.0f;
    float scrollOffset = 0.0f;
    const std::set<int>* selectedNotes = nullptr;

    int dragNoteIndex = -1;
};

} // namespace ui
} // namespace dc
