#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ButtonWidget.h"
#include <functional>
#include <vector>
#include <memory>

namespace dc
{
namespace ui
{

class PatternSelectorWidget : public gfx::Widget
{
public:
    PatternSelectorWidget();

    void resized() override;

    void setNumPatterns (int count);
    void setActivePattern (int index);
    int getActivePattern() const { return activeIndex; }

    std::function<void (int)> onPatternSelected;

private:
    int activeIndex = 0;
    std::vector<std::unique_ptr<gfx::ButtonWidget>> patternButtons;
};

} // namespace ui
} // namespace dc
