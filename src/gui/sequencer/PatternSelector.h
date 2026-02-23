#pragma once
#include <JuceHeader.h>
#include "model/Project.h"
#include "model/StepSequencer.h"

namespace dc
{

class PatternSelector : public juce::Component
{
public:
    PatternSelector (Project& project);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void rebuild();

    static constexpr int preferredHeight = 36;

private:
    Project& project;
    juce::OwnedArray<juce::TextButton> patternButtons;

    static constexpr int buttonWidth = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternSelector)
};

} // namespace dc
