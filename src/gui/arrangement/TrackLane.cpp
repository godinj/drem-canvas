#include "TrackLane.h"

namespace dc
{

TrackLane::TrackLane (const juce::ValueTree& state)
    : trackState (state)
{
    trackState.addListener (this);
    rebuildClipViews();
}

TrackLane::~TrackLane()
{
    trackState.removeListener (this);
}

void TrackLane::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Draw header area
    auto headerArea = bounds.removeFromLeft (headerWidth);

    auto trackColour = juce::Colour (static_cast<juce::uint32> (static_cast<int> (trackState.getProperty (IDs::colour, static_cast<int> (0xff4488aa)))));

    if (selected)
    {
        // Brighter header when selected
        g.setColour (trackColour.darker (0.2f));
        g.fillRect (headerArea);

        // Accent strip on left edge
        g.setColour (juce::Colour (0xff50c878));
        g.fillRect (headerArea.removeFromLeft (3));
    }
    else
    {
        g.setColour (trackColour.darker (0.5f));
        g.fillRect (headerArea);
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (14.0f));
    g.drawText (trackState.getProperty (IDs::name, "Untitled").toString(),
                headerArea.reduced (8, 0),
                juce::Justification::centredLeft,
                true);

    // Subtle green tint over lane body when selected
    if (selected)
    {
        g.setColour (juce::Colour (0xff50c878).withAlpha (0.06f));
        g.fillRect (bounds);
    }

    // Draw horizontal separator at bottom
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TrackLane::paintOverChildren (juce::Graphics& g)
{
    if (! selected || selectedClipIndex < 0 || selectedClipIndex >= clipViews.size())
        return;

    // Draw selection highlight around the selected clip
    auto* clipView = clipViews[selectedClipIndex];
    auto clipBounds = clipView->getBounds().toFloat();

    // Outer glow
    g.setColour (juce::Colour (0xff50c878).withAlpha (0.25f));
    g.fillRoundedRectangle (clipBounds.expanded (2.0f), 3.0f);

    // Selection border
    g.setColour (juce::Colour (0xff50c878));
    g.drawRoundedRectangle (clipBounds, 3.0f, 2.0f);
}

void TrackLane::resized()
{
    int clipIndex = 0;

    for (int i = 0; i < trackState.getNumChildren(); ++i)
    {
        auto child = trackState.getChild (i);

        if (child.hasType (IDs::AUDIO_CLIP))
        {
            if (clipIndex < clipViews.size())
            {
                double startPos = static_cast<double> (child.getProperty (IDs::startPosition, 0));
                double clipLength = static_cast<double> (child.getProperty (IDs::length, 0));

                int x = juce::roundToInt ((startPos / sampleRate) * pixelsPerSecond) + headerWidth;
                int w = juce::roundToInt ((clipLength / sampleRate) * pixelsPerSecond);

                clipViews[clipIndex]->setBounds (x, 0, w, getHeight());
            }

            ++clipIndex;
        }
    }
}

void TrackLane::setPixelsPerSecond (double pps)
{
    pixelsPerSecond = pps;
    resized();
}

void TrackLane::rebuildClipViews()
{
    clipViews.clear();

    auto trackColour = juce::Colour (static_cast<juce::uint32> (static_cast<int> (trackState.getProperty (IDs::colour, static_cast<int> (0xff4488aa)))));

    for (int i = 0; i < trackState.getNumChildren(); ++i)
    {
        auto child = trackState.getChild (i);

        if (child.hasType (IDs::AUDIO_CLIP))
        {
            auto* clipView = clipViews.add (new WaveformView());
            clipView->setWaveformColour (trackColour);

            juce::String filePath = child.getProperty (IDs::sourceFile, "").toString();

            if (filePath.isNotEmpty())
                clipView->setFile (juce::File (filePath));

            addAndMakeVisible (clipView);
        }
    }

    resized();
}

void TrackLane::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void TrackLane::setSelectedClipIndex (int index)
{
    if (selectedClipIndex != index)
    {
        selectedClipIndex = index;
        repaint();
    }
}

void TrackLane::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    repaint();
}

void TrackLane::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    rebuildClipViews();
}

void TrackLane::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    rebuildClipViews();
}

} // namespace dc
