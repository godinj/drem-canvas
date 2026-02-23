#include "StepButtonWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

StepButtonWidget::StepButtonWidget()
{
}

void StepButtonWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    Rect r (1, 1, w - 2, h - 2);

    if (active)
    {
        // Color based on velocity: gray→orange→red
        Color c;
        if (velocity < 40)
            c = Color::fromARGB (0xff606060);
        else if (velocity < 80)
            c = Color::fromARGB (0xffe09030);
        else if (velocity < 110)
            c = Color::fromARGB (0xffff6030);
        else
            c = Color::fromARGB (0xffff3030);

        canvas.fillRoundedRect (r, 2.0f, c);
    }
    else
    {
        canvas.fillRoundedRect (r, 2.0f, Color::fromARGB (0xff2a2a3a));
    }

    // Playback highlight (brighter overlay)
    if (playbackHighlighted)
        canvas.fillRoundedRect (r, 2.0f, Color (255, 255, 255, 60));

    // Cursor border (cyan)
    if (cursorHighlighted)
        canvas.strokeRect (Rect (0, 0, w, h), theme.cursor, 2.0f);

    // Beat separator on left edge
    if (beatSeparator)
        canvas.drawLine (0, 0, 0, h, theme.outlineColor, 2.0f);
}

void StepButtonWidget::mouseDown (const gfx::MouseEvent& e)
{
    if (onToggle)
        onToggle();
}

void StepButtonWidget::setActive (bool a) { active = a; repaint(); }
void StepButtonWidget::setVelocity (int v) { velocity = v; repaint(); }
void StepButtonWidget::setPlaybackHighlight (bool hl) { playbackHighlighted = hl; repaint(); }
void StepButtonWidget::setCursorHighlight (bool hl) { cursorHighlighted = hl; repaint(); }
void StepButtonWidget::setBeatSeparator (bool sep) { beatSeparator = sep; repaint(); }

} // namespace ui
} // namespace dc
