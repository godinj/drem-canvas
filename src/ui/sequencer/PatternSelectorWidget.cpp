#include "PatternSelectorWidget.h"
#include <string>

namespace dc
{
namespace ui
{

PatternSelectorWidget::PatternSelectorWidget()
{
}

void PatternSelectorWidget::resized()
{
    float buttonW = 40.0f;
    float margin = 4.0f;

    for (size_t i = 0; i < patternButtons.size(); ++i)
    {
        float x = static_cast<float> (i) * (buttonW + margin) + margin;
        patternButtons[i]->setBounds (x, 2, buttonW, getHeight() - 4.0f);
    }
}

void PatternSelectorWidget::setNumPatterns (int count)
{
    for (auto& btn : patternButtons)
        removeChild (btn.get());
    patternButtons.clear();

    for (int i = 0; i < count; ++i)
    {
        auto btn = std::make_unique<gfx::ButtonWidget> ("P" + std::to_string (i + 1));
        btn->setToggleable (true);
        btn->setToggleState (i == activeIndex);

        int idx = i;
        btn->onClick = [this, idx]()
        {
            setActivePattern (idx);
            if (onPatternSelected)
                onPatternSelected (idx);
        };

        addChild (btn.get());
        patternButtons.push_back (std::move (btn));
    }

    resized();
}

void PatternSelectorWidget::setActivePattern (int index)
{
    activeIndex = index;
    for (size_t i = 0; i < patternButtons.size(); ++i)
        patternButtons[i]->setToggleState (static_cast<int> (i) == activeIndex);
}

} // namespace ui
} // namespace dc
