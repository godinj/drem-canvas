#include "StepSequencerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

StepSequencerWidget::StepSequencerWidget (Project& proj)
    : project (proj)
{
    addChild (&patternSelector);
    addChild (&scrollView);
    scrollView.setContentWidget (&stepGrid);

    project.getState().addListener (this);
    setAnimating (true);
    rebuildFromModel();
}

StepSequencerWidget::~StepSequencerWidget()
{
    project.getState().removeListener (this);
}

void StepSequencerWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void StepSequencerWidget::resized()
{
    float w = getWidth();
    float h = getHeight();
    float selectorH = 30.0f;

    patternSelector.setBounds (0, 0, w, selectorH);
    scrollView.setBounds (0, selectorH, w, h - selectorH);
}

void StepSequencerWidget::animationTick (double /*timestampMs*/)
{
    // Update playback position from model if available
    repaint();
}

void StepSequencerWidget::rebuildFromModel()
{
    auto seqState = project.getState().getChildWithName (juce::Identifier ("STEP_SEQUENCER"));
    if (!seqState.isValid())
        return;

    StepSequencer seq (seqState);
    int numPatterns = seq.getNumPatterns();
    patternSelector.setNumPatterns (numPatterns);

    auto pattern = seq.getActivePattern();
    if (!pattern.isValid())
        return;

    int numSteps = static_cast<int> (pattern.getProperty ("numSteps", 16));
    int numRows = seq.getNumRows();

    stepGrid.setGrid (numRows, numSteps);

    // Set row labels and step states
    std::vector<std::string> labels;
    for (int r = 0; r < numRows; ++r)
    {
        auto row = seq.getRow (r);
        juce::String label = row.getProperty ("label", "Row " + juce::String (r + 1));
        labels.push_back (label.toStdString());

        for (int s = 0; s < numSteps; ++s)
        {
            auto step = StepSequencer::getStep (row, s);
            if (auto* btn = stepGrid.getButton (r, s))
            {
                btn->setActive (StepSequencer::isStepActive (step));
                btn->setVelocity (StepSequencer::getStepVelocity (step));
            }
        }
    }

    stepGrid.setRowLabels (labels);
    resized();
}

void StepSequencerWidget::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    rebuildFromModel();
}

} // namespace ui
} // namespace dc
