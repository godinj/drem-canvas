#include "CCLaneWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "utils/UndoSystem.h"

namespace dc
{
namespace ui
{

namespace
{
    const juce::Identifier ccPointId ("CC_POINT");
}

CCLaneWidget::CCLaneWidget (Project& p)
    : project (p)
{
}

void CCLaneWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    float w = getWidth();
    float h = getHeight();

    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff121220));

    // Top border
    canvas.fillRect (Rect (0, 0, w, 1.0f), Color::fromARGB (0xff383850));

    if (! clipState.isValid())
        return;

    // Collect CC points for current CC number
    struct CCPoint
    {
        double beat;
        int value;
    };
    std::vector<CCPoint> points;

    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto child = clipState.getChild (i);
        if (! child.hasType (ccPointId))
            continue;

        int cc = static_cast<int> (child.getProperty ("ccNumber", 1));
        if (cc != ccNumber)
            continue;

        double beat = static_cast<double> (child.getProperty ("beat", 0.0));
        int value = static_cast<int> (child.getProperty ("value", 0));
        points.push_back ({ beat, value });
    }

    // Sort by beat
    std::sort (points.begin(), points.end(),
               [] (const CCPoint& a, const CCPoint& b) { return a.beat < b.beat; });

    // Draw lines between points
    Color lineColor (100, 200, 255);

    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        float x1 = static_cast<float> (points[i].beat * pixelsPerBeat) - scrollOffset;
        float y1 = h - (static_cast<float> (points[i].value) / 127.0f) * (h - 4.0f) - 2.0f;
        float x2 = static_cast<float> (points[i + 1].beat * pixelsPerBeat) - scrollOffset;
        float y2 = h - (static_cast<float> (points[i + 1].value) / 127.0f) * (h - 4.0f) - 2.0f;

        canvas.drawLine (x1, y1, x2, y2, lineColor, 1.5f);
    }

    // Draw dots at each point
    for (auto& pt : points)
    {
        float x = static_cast<float> (pt.beat * pixelsPerBeat) - scrollOffset;
        float y = h - (static_cast<float> (pt.value) / 127.0f) * (h - 4.0f) - 2.0f;

        canvas.fillRoundedRect (Rect (x - 3.0f, y - 3.0f, 6.0f, 6.0f), 3.0f, lineColor);
    }
}

void CCLaneWidget::mouseDown (const gfx::MouseEvent& e)
{
    drawing = true;

    double beat = static_cast<double> (e.x + scrollOffset) / static_cast<double> (pixelsPerBeat);
    float h = getHeight();
    int value = static_cast<int> ((1.0f - (e.y / h)) * 127.0f);
    value = juce::jlimit (0, 127, value);

    addOrUpdateCCPoint (beat, value);
}

void CCLaneWidget::mouseDrag (const gfx::MouseEvent& e)
{
    if (! drawing)
        return;

    double beat = static_cast<double> (e.x + scrollOffset) / static_cast<double> (pixelsPerBeat);
    float h = getHeight();
    int value = static_cast<int> ((1.0f - (e.y / h)) * 127.0f);
    value = juce::jlimit (0, 127, value);

    addOrUpdateCCPoint (beat, value);
}

void CCLaneWidget::mouseUp (const gfx::MouseEvent&)
{
    drawing = false;
}

void CCLaneWidget::addOrUpdateCCPoint (double beat, int value)
{
    if (! clipState.isValid())
        return;

    project.getUndoSystem().beginCoalescedTransaction ("Edit CC");
    auto& um = project.getUndoManager();

    // Snap beat to nearest 1/16
    double gridSize = 1.0 / 16.0;
    beat = std::round (beat / gridSize) * gridSize;
    if (beat < 0.0)
        beat = 0.0;

    // Look for existing point at this beat for this CC number
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto child = clipState.getChild (i);
        if (! child.hasType (ccPointId))
            continue;

        int cc = static_cast<int> (child.getProperty ("ccNumber", 1));
        double existingBeat = static_cast<double> (child.getProperty ("beat", 0.0));

        if (cc == ccNumber && std::abs (existingBeat - beat) < 0.01)
        {
            child.setProperty ("value", value, &um);

            MidiClip clip (clipState);
            clip.collapseChildrenToMidiData (&um);
            return;
        }
    }

    // Add new CC point
    juce::ValueTree ccPoint (ccPointId);
    ccPoint.setProperty ("ccNumber", ccNumber, &um);
    ccPoint.setProperty ("beat", beat, &um);
    ccPoint.setProperty ("value", value, &um);
    clipState.appendChild (ccPoint, &um);

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

} // namespace ui
} // namespace dc
