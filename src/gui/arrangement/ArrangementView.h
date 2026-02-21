#pragma once
#include <JuceHeader.h>
#include "TimeRuler.h"
#include "TrackLane.h"
#include "model/Project.h"
#include "engine/TransportController.h"

namespace dc
{

class ArrangementView : public juce::Component,
                        private juce::Timer,
                        private juce::ValueTree::Listener
{
public:
    ArrangementView (Project& project, TransportController& transport);
    ~ArrangementView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}
    void rebuildTrackLanes();

    Project& project;
    TransportController& transportController;

    TimeRuler timeRuler;
    juce::OwnedArray<TrackLane> trackLanes;
    juce::Viewport viewport;
    juce::Component trackContainer;  // Goes inside viewport

    double pixelsPerSecond = 100.0;
    static constexpr int rulerHeight = 30;
    static constexpr int trackHeight = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementView)
};

} // namespace dc
