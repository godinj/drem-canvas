#include "LabelWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace gfx
{

LabelWidget::LabelWidget (const std::string& t, Alignment align)
    : text (t), alignment (align), textColor (Theme::getDefault().defaultText)
{
}

void LabelWidget::paint (Canvas& canvas)
{
    if (text.empty())
        return;

    auto& fm = FontManager::getInstance();
    const SkFont& font = (fontSize > 0.0f)
        ? (useMono ? fm.makeMonoFont (fontSize) : fm.makeFont (fontSize))
        : (useMono ? fm.getMonoFont() : fm.getDefaultFont());

    Rect r (0, 0, getWidth(), getHeight());

    switch (alignment)
    {
        case Left:
            canvas.drawText (text, 4.0f, r.height * 0.5f + font.getSize() * 0.35f, font, textColor);
            break;
        case Centre:
            canvas.drawTextCentred (text, r, font, textColor);
            break;
        case Right:
            canvas.drawTextRight (text, r, font, textColor);
            break;
    }
}

void LabelWidget::setText (const std::string& t)
{
    if (text != t)
    {
        text = t;
        repaint();
    }
}

void LabelWidget::setFontSize (float size)
{
    if (fontSize != size)
    {
        fontSize = size;
        repaint();
    }
}

} // namespace gfx
} // namespace dc
