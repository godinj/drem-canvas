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

    if (selected || inVisualSelection)
    {
        // Brighter header when selected
        g.setColour (trackColour.darker (0.2f));
        g.fillRect (headerArea);

        // Accent strip on left edge — orange for visual, green for normal
        g.setColour (inVisualSelection ? juce::Colour (0xffff9944) : juce::Colour (0xff50c878));
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

    // Subtle tint over lane body when selected or in visual selection
    if (selected || inVisualSelection)
    {
        auto tintColour = inVisualSelection ? juce::Colour (0xffff9944) : juce::Colour (0xff50c878);
        g.setColour (tintColour.withAlpha (0.06f));
        g.fillRect (bounds);
    }

    // Draw horizontal separator at bottom
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TrackLane::paintOverChildren (juce::Graphics& g)
{
    // Visual selection highlight
    if (inVisualSelection)
    {
        auto highlightColour = juce::Colour (0xffff9944); // orange

        if (visualLinewise)
        {
            // Linewise: highlight all clips
            for (auto* clipView : clipViews)
            {
                auto clipBounds = clipView->getBounds().toFloat();
                g.setColour (highlightColour.withAlpha (0.25f));
                g.fillRoundedRectangle (clipBounds.expanded (2.0f), 3.0f);
                g.setColour (highlightColour);
                g.drawRoundedRectangle (clipBounds, 3.0f, 2.0f);
            }
        }
        else
        {
            // Clipwise: highlight clips in visual range
            int start = std::max (0, visualStartClip);
            int end   = std::min (clipViews.size() - 1, visualEndClip);

            for (int i = start; i <= end; ++i)
            {
                auto* clipView = clipViews[i];
                auto clipBounds = clipView->getBounds().toFloat();
                g.setColour (highlightColour.withAlpha (0.25f));
                g.fillRoundedRectangle (clipBounds.expanded (2.0f), 3.0f);
                g.setColour (highlightColour);
                g.drawRoundedRectangle (clipBounds, 3.0f, 2.0f);
            }
        }
        return;
    }

    // Normal mode: single clip highlight
    if (! selected || selectedClipIndex < 0 || selectedClipIndex >= clipViews.size())
        return;

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

void TrackLane::setVisualSelection (const VimContext::VisualSelection& sel, int trackIndex)
{
    bool wasInVisual = inVisualSelection;

    if (sel.active)
    {
        int minTrack = std::min (sel.startTrack, sel.endTrack);
        int maxTrack = std::max (sel.startTrack, sel.endTrack);
        inVisualSelection = (trackIndex >= minTrack && trackIndex <= maxTrack);
        visualLinewise = sel.linewise;

        if (inVisualSelection && ! sel.linewise)
        {
            if (minTrack == maxTrack)
            {
                visualStartClip = std::min (sel.startClip, sel.endClip);
                visualEndClip   = std::max (sel.startClip, sel.endClip);
            }
            else if (trackIndex > minTrack && trackIndex < maxTrack)
            {
                // Intermediate track — all clips
                visualStartClip = 0;
                visualEndClip   = clipViews.size() - 1;
            }
            else
            {
                bool startIsMin = (sel.startTrack <= sel.endTrack);
                int anchorClip  = startIsMin ? sel.startClip : sel.endClip;
                int cursorClip  = startIsMin ? sel.endClip   : sel.startClip;

                if (trackIndex == minTrack)
                {
                    visualStartClip = anchorClip;
                    visualEndClip   = clipViews.size() - 1;
                }
                else // trackIndex == maxTrack
                {
                    visualStartClip = 0;
                    visualEndClip   = cursorClip;
                }
            }
        }
        else
        {
            visualStartClip = 0;
            visualEndClip   = clipViews.size() - 1;
        }
    }
    else
    {
        inVisualSelection = false;
        visualLinewise = false;
        visualStartClip = -1;
        visualEndClip = -1;
    }

    if (wasInVisual != inVisualSelection)
        repaint();
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
