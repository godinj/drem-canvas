#pragma once

#include "graphics/core/Widget.h"
#include "StepButtonWidget.h"
#include <vector>
#include <memory>
#include <string>

namespace dc
{
namespace ui
{

class StepGridWidget : public gfx::Widget
{
public:
    StepGridWidget();

    void paint (gfx::Canvas& canvas) override;
    void resized() override;

    void setGrid (int numRows, int numSteps);
    void setCursorPosition (int row, int step);
    void setPlaybackStep (int step);

    StepButtonWidget* getButton (int row, int step);

    void setRowLabels (const std::vector<std::string>& labels);

private:
    int numRows = 0;
    int numSteps = 0;
    int cursorRow = 0;
    int cursorStep = 0;
    int playbackStep = -1;

    static constexpr float labelWidth = 60.0f;
    static constexpr float cellSize = 28.0f;

    std::vector<std::string> rowLabels;
    std::vector<std::vector<std::unique_ptr<StepButtonWidget>>> buttons;
};

} // namespace ui
} // namespace dc
