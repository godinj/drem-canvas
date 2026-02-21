#pragma once
#include <JuceHeader.h>
#include "engine/TransportController.h"

namespace dc
{

class TransportBar : public juce::Component, private juce::Timer
{
public:
    TransportBar (TransportController& transport);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    TransportController& transportController;

    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::Label timeDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace dc
