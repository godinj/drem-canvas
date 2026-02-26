#include "NoteGridWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

NoteGridWidget::NoteGridWidget()
{
}

void NoteGridWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff1a1a2e));

    // Draw row backgrounds (alternating white/black key colors)
    for (int note = 0; note < 128; ++note)
    {
        float y = noteToY (note);
        if (y + rowHeight < 0 || y > h)
            continue;

        bool black = isBlackKey (note);
        Color rowColor = black ? Color::fromARGB (0xff161626) : Color::fromARGB (0xff1e1e32);

        // Highlight C rows
        if (note % 12 == 0)
            rowColor = Color::fromARGB (0xff222240);

        canvas.fillRect (Rect (0, y, w, rowHeight), rowColor);

        // Horizontal line between rows
        canvas.fillRect (Rect (0, y + rowHeight - 0.5f, w, 0.5f),
                         Color::fromARGB (0xff2a2a3e));
    }

    // Draw vertical grid lines (subdivisions, beats, bars)
    float beatsPerBar = static_cast<float> (timeSigNumerator);
    float totalBeats = w / pixelsPerBeat;

    // Subdivision lines
    float subdiv = 1.0f / static_cast<float> (gridDivision);
    for (float beat = 0.0f; beat < totalBeats; beat += subdiv)
    {
        float x = beatsToX (beat);
        if (x < 0 || x > w)
            continue;

        bool isBar = (std::fmod (beat, beatsPerBar) < 0.001f);
        bool isBeat = (std::fmod (beat, 1.0f) < 0.001f);

        if (isBar)
            canvas.fillRect (Rect (x, 0, 1.0f, h), Color::fromARGB (0xff505068));
        else if (isBeat)
            canvas.fillRect (Rect (x, 0, 0.5f, h), Color::fromARGB (0xff383850));
        else
            canvas.fillRect (Rect (x, 0, 0.5f, h), Color::fromARGB (0xff282840));
    }
}

void NoteGridWidget::paintOverChildren (gfx::Canvas& canvas)
{
    // Draw rubber band selection rect
    if (isRubberBanding)
    {
        using namespace gfx;
        float rx = std::min (rbStartX, rbEndX);
        float ry = std::min (rbStartY, rbEndY);
        float rw = std::abs (rbEndX - rbStartX);
        float rh = std::abs (rbEndY - rbStartY);

        canvas.fillRect (Rect (rx, ry, rw, rh), Color (100, 150, 255, 40));
        canvas.strokeRect (Rect (rx, ry, rw, rh), Color (100, 150, 255, 150), 1.0f);
    }
}

void NoteGridWidget::mouseDown (const gfx::MouseEvent& e)
{
    int note = yToNote (e.y);
    double beat = xToBeats (e.x);

    if (toolMode == DrawTool)
    {
        if (onDrawNote)
            onDrawNote (note, beat);
    }
    else if (toolMode == EraseTool)
    {
        if (onEraseNote)
            onEraseNote (note, beat);
    }
    else // SelectTool
    {
        // Start rubber band
        isRubberBanding = true;
        rbStartX = e.x;
        rbStartY = e.y;
        rbEndX = e.x;
        rbEndY = e.y;

        if (onEmptyClick)
            onEmptyClick();
    }
}

void NoteGridWidget::mouseDrag (const gfx::MouseEvent& e)
{
    if (isRubberBanding)
    {
        rbEndX = e.x;
        rbEndY = e.y;
        repaint();
    }
}

void NoteGridWidget::mouseUp (const gfx::MouseEvent& e)
{
    if (isRubberBanding)
    {
        isRubberBanding = false;

        float rx = std::min (rbStartX, rbEndX);
        float ry = std::min (rbStartY, rbEndY);
        float rw = std::abs (rbEndX - rbStartX);
        float rh = std::abs (rbEndY - rbStartY);

        if (rw > 3.0f && rh > 3.0f && onRubberBandSelect)
            onRubberBandSelect (rx, ry, rw, rh);

        repaint();
    }
}

float NoteGridWidget::beatsToX (double beats) const
{
    return static_cast<float> (beats * pixelsPerBeat);
}

double NoteGridWidget::xToBeats (float x) const
{
    return static_cast<double> (x) / static_cast<double> (pixelsPerBeat);
}

float NoteGridWidget::noteToY (int noteNumber) const
{
    return static_cast<float> (127 - noteNumber) * rowHeight;
}

int NoteGridWidget::yToNote (float y) const
{
    return 127 - static_cast<int> (y / rowHeight);
}

bool NoteGridWidget::isBlackKey (int note) const
{
    int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

} // namespace ui
} // namespace dc
