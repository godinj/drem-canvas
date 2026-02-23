#pragma once

#include "graphics/core/Widget.h"

namespace dc
{
namespace gfx
{

class LayoutWidget : public Widget
{
public:
    enum Direction { Horizontal, Vertical };

    explicit LayoutWidget (Direction dir = Vertical);

    void resized() override;

    void setDirection (Direction dir) { direction = dir; resized(); }
    Direction getDirection() const { return direction; }

    void setSpacing (float s) { spacing = s; resized(); }
    float getSpacing() const { return spacing; }

    // Add child with fixed size or stretch factor
    void addFixedChild (Widget* child, float size);
    void addStretchChild (Widget* child, float stretchFactor = 1.0f);

    struct ChildLayout
    {
        Widget* widget = nullptr;
        float fixedSize = 0.0f;     // 0 = stretch
        float stretchFactor = 1.0f;
    };

private:
    Direction direction;
    float spacing = 0.0f;
    std::vector<ChildLayout> childLayouts;
};

} // namespace gfx
} // namespace dc
