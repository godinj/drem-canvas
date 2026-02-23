#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ScrollViewWidget.h"
#include "StepGridWidget.h"
#include "PatternSelectorWidget.h"
#include "model/Project.h"
#include "model/StepSequencer.h"
#include <JuceHeader.h>

namespace dc
{
namespace ui
{

class StepSequencerWidget : public gfx::Widget
{
public:
    explicit StepSequencerWidget (Project& project);
    ~StepSequencerWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void resized() override;
    void animationTick (double timestampMs) override;

    void rebuildFromModel();

    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}

private:
    Project& project;

    PatternSelectorWidget patternSelector;
    gfx::ScrollViewWidget scrollView;
    StepGridWidget stepGrid;
};

} // namespace ui
} // namespace dc
