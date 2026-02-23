#include "StepSequencerView.h"

namespace dc
{

StepSequencerView::StepSequencerView (Project& p, StepSequencerProcessor* proc)
    : project (p),
      sequencerProcessor (proc),
      patternSelector (p),
      grid (p)
{
    addAndMakeVisible (patternSelector);
    gridViewport.setViewedComponent (&grid, false);
    gridViewport.setScrollBarsShown (true, true);
    addAndMakeVisible (gridViewport);

    startTimerHz (30);
}

StepSequencerView::~StepSequencerView()
{
    stopTimer();
}

void StepSequencerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2a));
}

void StepSequencerView::resized()
{
    auto area = getLocalBounds();
    patternSelector.setBounds (area.removeFromTop (PatternSelector::preferredHeight));
    gridViewport.setBounds (area);
}

void StepSequencerView::timerCallback()
{
    if (sequencerProcessor != nullptr)
    {
        int step = sequencerProcessor->getCurrentStep();
        grid.setPlaybackStep (step);
    }
}

} // namespace dc
