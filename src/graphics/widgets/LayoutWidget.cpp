#include "LayoutWidget.h"

namespace dc
{
namespace gfx
{

LayoutWidget::LayoutWidget (Direction dir)
    : direction (dir)
{
}

void LayoutWidget::addFixedChild (Widget* child, float size)
{
    if (!child) return;
    addChild (child);
    childLayouts.push_back ({ child, size, 0.0f });
    resized();
}

void LayoutWidget::addStretchChild (Widget* child, float stretchFactor)
{
    if (!child) return;
    addChild (child);
    childLayouts.push_back ({ child, 0.0f, stretchFactor });
    resized();
}

void LayoutWidget::resized()
{
    if (childLayouts.empty())
        return;

    float totalSpace = (direction == Horizontal) ? getWidth() : getHeight();
    float crossSize = (direction == Horizontal) ? getHeight() : getWidth();
    float totalSpacing = spacing * static_cast<float> (childLayouts.size() - 1);
    float availableSpace = totalSpace - totalSpacing;

    // Calculate total fixed size and total stretch
    float totalFixed = 0.0f;
    float totalStretch = 0.0f;
    for (auto& cl : childLayouts)
    {
        if (cl.fixedSize > 0.0f)
            totalFixed += cl.fixedSize;
        else
            totalStretch += cl.stretchFactor;
    }

    float stretchSpace = availableSpace - totalFixed;
    float pos = 0.0f;

    for (auto& cl : childLayouts)
    {
        float size;
        if (cl.fixedSize > 0.0f)
            size = cl.fixedSize;
        else
            size = (totalStretch > 0.0f) ? (cl.stretchFactor / totalStretch) * stretchSpace : 0.0f;

        if (direction == Horizontal)
            cl.widget->setBounds (pos, 0, size, crossSize);
        else
            cl.widget->setBounds (0, pos, crossSize, size);

        pos += size + spacing;
    }
}

} // namespace gfx
} // namespace dc
