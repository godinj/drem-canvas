#include "PianoKeyboardWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace ui
{

PianoKeyboardWidget::PianoKeyboardWidget()
{
}

void PianoKeyboardWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getSmallFont();
    float w = getWidth();

    canvas.fillRect (Rect (0, 0, w, getHeight()), Color::fromARGB (0xff1a1a2a));

    for (int note = 0; note < 128; ++note)
    {
        float y = (127 - note) * rowHeight - scrollOffset;
        if (y + rowHeight < 0 || y > getHeight()) continue;

        bool black = isBlackKey (note);
        Color keyColor = black ? Color::fromARGB (0xff2a2a3a) : Color::fromARGB (0xffdedede);
        Color textColor = black ? theme.dimText : Color::fromARGB (0xff2a2a3a);

        canvas.fillRect (Rect (0, y, w, rowHeight - 1.0f), keyColor);

        // Draw C note labels
        if (note % 12 == 0)
        {
            int octave = note / 12 - 1;
            canvas.drawText ("C" + std::to_string (octave),
                             2.0f, y + rowHeight * 0.5f + 3.0f, font, textColor);
        }
    }
}

void PianoKeyboardWidget::mouseDown (const gfx::MouseEvent& e)
{
    int note = noteFromY (e.y);
    if (note >= 0 && note < 128 && onNoteClicked)
        onNoteClicked (note);
}

bool PianoKeyboardWidget::isBlackKey (int note) const
{
    int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

int PianoKeyboardWidget::noteFromY (float y) const
{
    return 127 - static_cast<int> ((y + scrollOffset) / rowHeight);
}

} // namespace ui
} // namespace dc
