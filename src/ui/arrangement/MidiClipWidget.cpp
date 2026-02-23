#include "MidiClipWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

MidiClipWidget::MidiClipWidget (const juce::ValueTree& state)
    : clipState (state)
{
}

void MidiClipWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Clip background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff2a3a4a));

    // Draw miniature note bars from MIDI data
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto note = clipState.getChild (i);
        if (note.getType().toString() != "NOTE")
            continue;

        int noteNum = static_cast<int> (note.getProperty ("noteNumber", 60));
        auto startTick = static_cast<double> (note.getProperty ("startBeat", 0.0));
        auto lengthTick = static_cast<double> (note.getProperty ("lengthBeats", 0.25));

        // Normalized positions
        float noteY = h - (static_cast<float> (noteNum) / 127.0f) * h;
        float noteX = static_cast<float> (startTick / 4.0) * w;  // 4 beats per visible area
        float noteW = std::max (2.0f, static_cast<float> (lengthTick / 4.0) * w);
        float noteH = std::max (1.0f, h / 64.0f);

        canvas.fillRect (Rect (noteX, noteY - noteH * 0.5f, noteW, noteH), theme.accent);
    }

    // Border
    canvas.strokeRect (Rect (0, 0, w, h), theme.outlineColor, 1.0f);
}

} // namespace ui
} // namespace dc
