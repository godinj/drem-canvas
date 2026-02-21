#pragma once
#include <JuceHeader.h>
#include "TimeRuler.h"
#include "TrackLane.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "engine/TransportController.h"

namespace dc
{

class ArrangementView : public juce::Component,
                        public VimEngine::Listener,
                        private juce::Timer,
                        private juce::ValueTree::Listener
{
public:
    ArrangementView (Project& project, TransportController& transport,
                     Arrangement& arrangement, VimContext& vimContext);
    ~ArrangementView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void rebuildTrackLanes();

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

private:
    void timerCallback() override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    void updateSelectionVisuals();
    void ensureSelectedTrackVisible();

    Project& project;
    TransportController& transportController;
    Arrangement& arrangement;
    VimContext& vimContext;

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
