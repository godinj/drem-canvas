#include "ArrangementView.h"

namespace dc
{

ArrangementView::ArrangementView (Project& proj, TransportController& transport)
    : project (proj),
      transportController (transport)
{
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

    resized();
}

void ArrangementView::paint (juce::Graphics& g)
{
    // Background
    g.setColour (juce::Colour (0xff1a1a2a));
    g.fillAll();

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

} // namespace dc
