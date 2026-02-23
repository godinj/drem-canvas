#pragma once
#include <JuceHeader.h>
#include "StepGrid.h"
#include "PatternSelector.h"
#include "model/Project.h"
#include "engine/StepSequencerProcessor.h"

namespace dc
{

class StepSequencerView : public juce::Component,
                          private juce::Timer
{
public:
    StepSequencerView (Project& project, StepSequencerProcessor* processor);
    ~StepSequencerView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    StepGrid& getGrid() { return grid; }

private:
    void timerCallback() override;

    Project& project;
    StepSequencerProcessor* sequencerProcessor;

    PatternSelector patternSelector;
    StepGrid grid;

    juce::Viewport gridViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerView)
};

} // namespace dc
