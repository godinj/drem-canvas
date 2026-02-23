#include "StepGridWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace ui
{

StepGridWidget::StepGridWidget()
{
}

void StepGridWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getSmallFont();

    // Draw row labels
    for (int r = 0; r < numRows; ++r)
    {
        float y = static_cast<float> (r) * cellSize;
        if (r < static_cast<int> (rowLabels.size()))
        {
            canvas.drawText (rowLabels[static_cast<size_t> (r)], 4.0f, y + cellSize * 0.5f + 4.0f,
                             font, theme.defaultText);
        }
    }
}

void StepGridWidget::resized()
{
    for (int r = 0; r < numRows; ++r)
    {
        for (int s = 0; s < numSteps; ++s)
        {
            float x = labelWidth + static_cast<float> (s) * cellSize;
            float y = static_cast<float> (r) * cellSize;

            auto* btn = getButton (r, s);
            if (btn)
            {
                btn->setBounds (x, y, cellSize, cellSize);
                btn->setBeatSeparator (s % 4 == 0 && s > 0);
                btn->setCursorHighlight (r == cursorRow && s == cursorStep);
                btn->setPlaybackHighlight (s == playbackStep);
            }
        }
    }
}

void StepGridWidget::setGrid (int rows, int steps)
{
    // Remove old buttons
    for (auto& row : buttons)
        for (auto& btn : row)
            removeChild (btn.get());
    buttons.clear();

    numRows = rows;
    numSteps = steps;

    buttons.resize (static_cast<size_t> (rows));
    for (int r = 0; r < rows; ++r)
    {
        buttons[static_cast<size_t> (r)].resize (static_cast<size_t> (steps));
        for (int s = 0; s < steps; ++s)
        {
            auto btn = std::make_unique<StepButtonWidget>();
            addChild (btn.get());
            buttons[static_cast<size_t> (r)][static_cast<size_t> (s)] = std::move (btn);
        }
    }

    resized();
}

void StepGridWidget::setCursorPosition (int row, int step)
{
    // Clear old cursor
    if (auto* old = getButton (cursorRow, cursorStep))
        old->setCursorHighlight (false);

    cursorRow = row;
    cursorStep = step;

    if (auto* btn = getButton (cursorRow, cursorStep))
        btn->setCursorHighlight (true);
}

void StepGridWidget::setPlaybackStep (int step)
{
    // Clear old
    if (playbackStep >= 0)
        for (int r = 0; r < numRows; ++r)
            if (auto* btn = getButton (r, playbackStep))
                btn->setPlaybackHighlight (false);

    playbackStep = step;

    if (playbackStep >= 0)
        for (int r = 0; r < numRows; ++r)
            if (auto* btn = getButton (r, playbackStep))
                btn->setPlaybackHighlight (true);
}

StepButtonWidget* StepGridWidget::getButton (int row, int step)
{
    if (row >= 0 && row < numRows && step >= 0 && step < numSteps)
        return buttons[static_cast<size_t> (row)][static_cast<size_t> (step)].get();
    return nullptr;
}

void StepGridWidget::setRowLabels (const std::vector<std::string>& labels)
{
    rowLabels = labels;
    repaint();
}

} // namespace ui
} // namespace dc
