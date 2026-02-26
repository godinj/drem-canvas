#include "PianoRollRulerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace ui
{

PianoRollRulerWidget::PianoRollRulerWidget()
{
}

void PianoRollRulerWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getSmallFont();

    float w = getWidth();
    float h = getHeight();

    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff1a1a2a));

    float beatsPerBar = static_cast<float> (timeSigNumerator);
    float totalBeats = (w + scrollOffset) / pixelsPerBeat;

    for (float beat = 0.0f; beat < totalBeats; beat += 1.0f)
    {
        float x = beat * pixelsPerBeat - scrollOffset;
        if (x < 0 || x > w)
            continue;

        bool isBar = (std::fmod (beat, beatsPerBar) < 0.001f);

        if (isBar)
        {
            int barNum = static_cast<int> (beat / beatsPerBar) + 1;
            canvas.fillRect (Rect (x, 0, 1.0f, h), Color::fromARGB (0xff505068));
            canvas.drawText (std::to_string (barNum),
                             x + 3.0f, h - 6.0f, font, theme.brightText);
        }
        else
        {
            canvas.fillRect (Rect (x, h * 0.6f, 0.5f, h * 0.4f),
                             Color::fromARGB (0xff383850));
        }
    }

    // Bottom border
    canvas.fillRect (Rect (0, h - 1.0f, w, 1.0f), Color::fromARGB (0xff383850));
}

void PianoRollRulerWidget::mouseDown (const gfx::MouseEvent& e)
{
    double beat = static_cast<double> (e.x + scrollOffset) / static_cast<double> (pixelsPerBeat);
    if (beat < 0.0)
        beat = 0.0;

    if (onSeek)
        onSeek (beat);
}

} // namespace ui
} // namespace dc
