#include "VelocityLaneWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "utils/UndoSystem.h"

namespace dc
{
namespace ui
{

VelocityLaneWidget::VelocityLaneWidget (Project& p)
    : project (p)
{
}

void VelocityLaneWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    float w = getWidth();
    float h = getHeight();

    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff141420));

    // Top border
    canvas.fillRect (Rect (0, 0, w, 1.0f), Color::fromARGB (0xff383850));

    if (! clipState.isValid())
        return;

    float barWidth = 6.0f;
    int noteChildIdx = 0;

    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto child = clipState.getChild (i);
        if (! child.hasType (juce::Identifier ("NOTE")))
            continue;

        auto startBeat = static_cast<double> (child.getProperty ("startBeat", 0.0));
        int velocity = static_cast<int> (child.getProperty ("velocity", 100));

        float x = static_cast<float> (startBeat * pixelsPerBeat) - scrollOffset;
        float velNorm = static_cast<float> (velocity) / 127.0f;
        float barH = velNorm * (h - 4.0f);

        if (x + barWidth < 0 || x > w)
        {
            ++noteChildIdx;
            continue;
        }

        // Color based on velocity (matches NoteWidget)
        uint8_t r = static_cast<uint8_t> (74 + velNorm * 100);
        uint8_t g = static_cast<uint8_t> (158 - velNorm * 50);
        uint8_t b = 255;

        // Highlight selected notes
        bool selected = selectedNotes && selectedNotes->count (i) > 0;
        if (selected)
        {
            r = 255;
            g = 200;
            b = 50;
        }

        canvas.fillRect (Rect (x, h - barH - 1.0f, barWidth, barH), Color (r, g, b));
        ++noteChildIdx;
    }
}

void VelocityLaneWidget::mouseDown (const gfx::MouseEvent& e)
{
    if (! clipState.isValid())
        return;

    float barWidth = 6.0f;
    dragNoteIndex = -1;

    // Find the note under the click
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto child = clipState.getChild (i);
        if (! child.hasType (juce::Identifier ("NOTE")))
            continue;

        auto startBeat = static_cast<double> (child.getProperty ("startBeat", 0.0));
        float x = static_cast<float> (startBeat * pixelsPerBeat) - scrollOffset;

        if (e.x >= x && e.x <= x + barWidth)
        {
            dragNoteIndex = i;
            break;
        }
    }

    if (dragNoteIndex >= 0)
    {
        float h = getHeight();
        int vel = static_cast<int> ((1.0f - (e.y / h)) * 127.0f);
        vel = juce::jlimit (1, 127, vel);

        project.getUndoSystem().beginCoalescedTransaction ("Edit Velocity");
        clipState.getChild (dragNoteIndex).setProperty ("velocity", vel, &project.getUndoManager());

        MidiClip clip (clipState);
        clip.collapseChildrenToMidiData (&project.getUndoManager());
    }
}

void VelocityLaneWidget::mouseDrag (const gfx::MouseEvent& e)
{
    if (dragNoteIndex >= 0 && dragNoteIndex < clipState.getNumChildren())
    {
        float h = getHeight();
        int vel = static_cast<int> ((1.0f - (e.y / h)) * 127.0f);
        vel = juce::jlimit (1, 127, vel);

        project.getUndoSystem().beginCoalescedTransaction ("Edit Velocity");
        clipState.getChild (dragNoteIndex).setProperty ("velocity", vel, &project.getUndoManager());

        MidiClip clip (clipState);
        clip.collapseChildrenToMidiData (&project.getUndoManager());
    }
}

void VelocityLaneWidget::mouseUp (const gfx::MouseEvent&)
{
    dragNoteIndex = -1;
}

} // namespace ui
} // namespace dc
