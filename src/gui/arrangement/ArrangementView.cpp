#include "ArrangementView.h"

namespace dc
{

ArrangementView::ArrangementView (Project& proj, TransportController& transport,
                                   Arrangement& arr, VimContext& vc)
    : project (proj),
      transportController (transport),
      arrangement (arr),
      vimContext (vc)
{
    timeRuler.onSeek = [this] (double timeInSeconds)
    {
        double sr = transportController.getSampleRate();
        if (sr > 0.0)
            transportController.setPositionInSamples (static_cast<int64_t> (timeInSeconds * sr));
    };
    addAndMakeVisible (timeRuler);
    addAndMakeVisible (viewport);

    viewport.setViewedComponent (&trackContainer, false);
    viewport.setScrollBarsShown (true, true);

    // Listen to the TRACKS subtree for child additions/removals
    auto tracksTree = project.getState().getChildWithName (IDs::TRACKS);

    if (tracksTree.isValid())
        tracksTree.addListener (this);

    rebuildTrackLanes();

    // Start timer at 30Hz for playback cursor animation
    startTimerHz (30);
}

ArrangementView::~ArrangementView()
{
    stopTimer();

    auto tracksTree = project.getState().getChildWithName (IDs::TRACKS);

    if (tracksTree.isValid())
        tracksTree.removeListener (this);
}

void ArrangementView::rebuildTrackLanes()
{
    trackLanes.clear();
    trackContainer.removeAllChildren();

    double sr = project.getSampleRate();

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        auto trackState = project.getTrack (i);
        auto* lane = trackLanes.add (new TrackLane (trackState));
        lane->setPixelsPerSecond (pixelsPerSecond);
        lane->setSampleRate (sr);
        trackContainer.addAndMakeVisible (lane);
    }

    updateSelectionVisuals();
    resized();
}

void ArrangementView::paint (juce::Graphics& g)
{
    // Background
    g.setColour (juce::Colour (0xff1a1a2a));
    g.fillAll();
}

void ArrangementView::paintOverChildren (juce::Graphics& g)
{
    // Draw playback cursor
    double sr = transportController.getSampleRate();

    if (sr > 0.0)
    {
        double posInSamples = static_cast<double> (transportController.getPositionInSamples());
        double posInSeconds = posInSamples / sr;

        // Account for header width and scroll offset
        float cursorX = static_cast<float> (posInSeconds * pixelsPerSecond)
                        + 150.0f  // headerWidth from TrackLane
                        - static_cast<float> (viewport.getViewPositionX());

        if (cursorX >= 150.0f && cursorX <= static_cast<float> (getWidth()))
        {
            g.setColour (juce::Colours::red);
            g.drawVerticalLine (juce::roundToInt (cursorX),
                                static_cast<float> (rulerHeight),
                                static_cast<float> (getHeight()));
        }
    }

    // Active context indicator
    if (activeContext)
    {
        // Green top bar
        g.setColour (juce::Colour (0xff50c878));
        g.fillRect (0, 0, getWidth(), 2);
    }
    else
    {
        // Dark overlay for inactive panel
        g.setColour (juce::Colour (0x28000000));
        g.fillRect (getLocalBounds());
    }
}

void ArrangementView::setActiveContext (bool active)
{
    if (activeContext != active)
    {
        activeContext = active;
        repaint();
    }
}

void ArrangementView::resized()
{
    auto bounds = getLocalBounds();

    timeRuler.setBounds (bounds.removeFromTop (rulerHeight));
    viewport.setBounds (bounds);

    // Size the track container
    int containerWidth = juce::jmax (viewport.getWidth(), 10000);
    int containerHeight = static_cast<int> (trackLanes.size()) * trackHeight;

    trackContainer.setSize (containerWidth, juce::jmax (containerHeight, viewport.getHeight()));

    // Layout each track lane
    for (int i = 0; i < trackLanes.size(); ++i)
    {
        trackLanes[i]->setBounds (0, i * trackHeight, trackContainer.getWidth(), trackHeight);
    }
}

void ArrangementView::timerCallback()
{
    timeRuler.setScrollOffset (static_cast<double> (viewport.getViewPositionX()));
    repaint();
}

void ArrangementView::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildTrackLanes();
}

void ArrangementView::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildTrackLanes();
}

// ── VimEngine::Listener ─────────────────────────────────────────────────────

void ArrangementView::vimModeChanged (VimEngine::Mode)
{
    // Could change visual style per mode in the future
}

void ArrangementView::vimContextChanged()
{
    updateSelectionVisuals();
    ensureSelectedTrackVisible();
}

void ArrangementView::updateSelectionVisuals()
{
    int selectedTrack = arrangement.getSelectedTrackIndex();
    int selectedClip  = vimContext.getSelectedClipIndex();

    for (int i = 0; i < trackLanes.size(); ++i)
    {
        bool isSelected = (i == selectedTrack);
        trackLanes[i]->setSelected (isSelected);
        trackLanes[i]->setSelectedClipIndex (isSelected ? selectedClip : -1);
    }
}

void ArrangementView::ensureSelectedTrackVisible()
{
    int idx = arrangement.getSelectedTrackIndex();

    if (idx < 0 || idx >= trackLanes.size())
        return;

    // Compute the track's y-range in the track container
    int trackTop    = idx * trackHeight;
    int trackBottom = trackTop + trackHeight;

    // Current visible range in the viewport
    int viewTop    = viewport.getViewPositionY();
    int viewBottom = viewTop + viewport.getViewHeight();

    if (trackTop < viewTop)
        viewport.setViewPosition (viewport.getViewPositionX(), trackTop);
    else if (trackBottom > viewBottom)
        viewport.setViewPosition (viewport.getViewPositionX(), trackBottom - viewport.getViewHeight());
}

} // namespace dc
