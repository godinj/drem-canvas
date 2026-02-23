#include "VimStatusBar.h"
#include "model/Track.h"

namespace dc
{

VimStatusBar::VimStatusBar (VimEngine& e, VimContext& c,
                            Arrangement& a, TransportController& t)
    : engine (e), context (c), arrangement (a), transport (t)
{
    engine.addListener (this);
    startTimerHz (10);
}

VimStatusBar::~VimStatusBar()
{
    engine.removeListener (this);
}

void VimStatusBar::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff181825));

    auto font = juce::Font (juce::FontOptions (14.0f));
    g.setFont (font);

    // ── Mode segment ────────────────────────────────────────────────────
    auto modeArea = area.removeFromLeft (160);
    bool isNormal = engine.getMode() == VimEngine::Normal;
    auto modeColour = isNormal ? juce::Colour (0xff50c878)   // green
                               : juce::Colour (0xff4a9eff);  // blue
    g.setColour (modeColour);
    g.fillRect (modeArea);

    g.setColour (juce::Colour (0xff181825));
    auto modeText = isNormal ? "-- NORMAL --" : "-- INSERT --";
    g.drawText (modeText, modeArea.reduced (6, 0), juce::Justification::centredLeft);

    // ── Pending state indicator ─────────────────────────────────────────
    if (engine.hasPendingState())
    {
        auto pendingArea = area.removeFromLeft (80);
        g.setColour (juce::Colour (0xffffcc00)); // yellow
        g.drawText (engine.getPendingDisplay(), pendingArea.reduced (4, 0),
                    juce::Justification::centredLeft);
    }

    // ── Context panel segment ───────────────────────────────────────────
    auto panelArea = area.removeFromLeft (100);
    g.setColour (juce::Colour (0xffcdd6f4));
    g.drawText (context.getPanelName(), panelArea.reduced (6, 0),
                juce::Justification::centredLeft);

    // ── Cursor info segment ─────────────────────────────────────────────
    auto cursorArea = area.removeFromLeft (200);
    int trackIdx = arrangement.getSelectedTrackIndex();
    juce::String cursorText;

    if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
    {
        Track track = arrangement.getTrack (trackIdx);
        cursorText = "T" + juce::String (trackIdx + 1) + ":"
                   + track.getName()
                   + " C" + juce::String (context.getSelectedClipIndex() + 1);
    }
    else
    {
        cursorText = "No track selected";
    }

    g.setColour (juce::Colour (0xffa6adc8));
    g.drawText (cursorText, cursorArea.reduced (6, 0),
                juce::Justification::centredLeft);

    // ── Playhead info (right-aligned) ───────────────────────────────────
    auto playheadArea = area;
    g.setColour (juce::Colour (0xffa6adc8));
    g.drawText (transport.getTimeString(), playheadArea.reduced (6, 0),
                juce::Justification::centredRight);
}

void VimStatusBar::vimModeChanged (VimEngine::Mode)
{
    repaint();
}

void VimStatusBar::vimContextChanged()
{
    repaint();
}

void VimStatusBar::timerCallback()
{
    repaint();
}

} // namespace dc
