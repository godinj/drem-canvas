#include "NoteWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

NoteWidget::NoteWidget()
{
}

void NoteWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Color based on velocity
    float velNorm = static_cast<float> (velocity) / 127.0f;
    uint8_t r = static_cast<uint8_t> (74 + velNorm * 100);
    uint8_t g = static_cast<uint8_t> (158 - velNorm * 50);
    uint8_t b = 255;
    Color noteColor (r, g, b);

    canvas.fillRoundedRect (Rect (0, 0, w, h), 2.0f, noteColor);

    if (selected)
        canvas.strokeRect (Rect (0, 0, w, h), theme.brightText, 2.0f);

    // Resize handle on right edge
    canvas.fillRect (Rect (w - 3.0f, 0, 3.0f, h), noteColor.withAlpha ((uint8_t) 180));
}

void NoteWidget::mouseDown (const gfx::MouseEvent& e)
{
    dragStartX = e.x;
    dragStartY = e.y;

    // Right edge = resize
    if (e.x > getWidth() - 6.0f)
        resizing = true;
    else
        dragging = true;
}

void NoteWidget::mouseDrag (const gfx::MouseEvent& e)
{
    if (resizing)
    {
        if (onResize)
            onResize (e.x);
    }
    else if (dragging)
    {
        float dx = e.x - dragStartX;
        float dy = e.y - dragStartY;
        if (onDrag)
            onDrag (dx, dy);
    }
}

void NoteWidget::mouseUp (const gfx::MouseEvent& e)
{
    dragging = false;
    resizing = false;
}

} // namespace ui
} // namespace dc
