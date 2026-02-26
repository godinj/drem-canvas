#pragma once
#include <JuceHeader.h>
#include "engine/TransportController.h"
#include "model/Project.h"
#include "model/TempoMap.h"

namespace dc
{

class TransportBar : public juce::Component,
                     private juce::Timer,
                     private juce::Label::Listener
{
public:
    TransportBar (TransportController& transport, Project& project, TempoMap& tempoMap);

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Callback for metronome toggle — wired by MainComponent
    std::function<void (bool)> onMetronomeToggled;

private:
    void timerCallback() override;

    // Label::Listener
    void labelTextChanged (juce::Label* labelThatHasChanged) override;

    TransportController& transportController;
    Project& project;
    TempoMap& tempoMap;

    juce::TextButton rtzButton { "|<" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton playButton { "Play" };
    juce::TextButton recordButton { "Rec" };

    juce::Label timeDisplay;
    bool showBarsBeatsTicks = false;

    juce::Label tempoDisplay;
    juce::Label timeSigDisplay;

    juce::TextButton metronomeButton { "Click" };
    juce::TextButton loopButton { "Loop" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace dc
