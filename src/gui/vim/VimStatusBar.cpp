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

    // ── Command mode — full-width command line ────────────────────────
    if (engine.getMode() == VimEngine::Command)
    {
        g.setColour (juce::Colour (0xffcdd6f4));
        g.drawText (":" + engine.getCommandBuffer(), area.reduced (6, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // ── Mode segment ────────────────────────────────────────────────────
    auto modeArea = area.removeFromLeft (160);
    juce::Colour modeColour;
    const char* modeText;

    switch (engine.getMode())
    {
        case VimEngine::Normal:     modeColour = juce::Colour (0xff50c878); modeText = "-- NORMAL --"; break;
        case VimEngine::Insert:     modeColour = juce::Colour (0xff4a9eff); modeText = "-- INSERT --"; break;
        case VimEngine::PluginMenu: modeColour = juce::Colour (0xffcba6f7); modeText = "-- PLUGIN --"; break;
        case VimEngine::Visual:     modeColour = juce::Colour (0xffff9944); modeText = "-- VISUAL --"; break;
        case VimEngine::VisualLine: modeColour = juce::Colour (0xffff9944); modeText = "-- V-LINE --"; break;
        default:                    modeColour = juce::Colour (0xff50c878); modeText = "-- NORMAL --"; break;
    }

    g.setColour (modeColour);
    g.fillRect (modeArea);

    g.setColour (juce::Colour (0xff181825));
    g.drawText (modeText, modeArea.reduced (6, 0), juce::Justification::centredLeft);

    // ── Pending state indicator ─────────────────────────────────────────
    if (engine.hasPendingState())
    {
        auto pendingArea = area.removeFromLeft (80);
        g.setColour (juce::Colour (0xffffcc00)); // yellow
        g.drawText (engine.getPendingDisplay(), pendingArea.reduced (4, 0),
                    juce::Justification::centredLeft);
    }

    // ── Context panel segment (prominent green on dark bg) ─────────────
    auto panelArea = area.removeFromLeft (120);
    g.setColour (juce::Colour (0xff202030));
    g.fillRect (panelArea);
    g.setColour (juce::Colour (0xff50c878));
    g.drawText (context.getPanelName(), panelArea.reduced (6, 0),
                juce::Justification::centredLeft);

    // ── Breadcrumb info segment (context-dependent) ─────────────────────
    auto cursorArea = area.removeFromLeft (280);
    int trackIdx = arrangement.getSelectedTrackIndex();
    juce::String breadcrumb;

    if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
    {
        Track track = arrangement.getTrack (trackIdx);
        juce::String trackInfo = "T" + juce::String (trackIdx + 1) + ":"
                               + track.getName();

        auto panel = context.getPanel();
        auto& visSel = context.getVisualSelection();
        if (panel == VimContext::Editor && visSel.active)
        {
            int minT = std::min (visSel.startTrack, visSel.endTrack) + 1;
            int maxT = std::max (visSel.startTrack, visSel.endTrack) + 1;

            if (visSel.linewise)
            {
                breadcrumb = "> T" + juce::String (minT) + "-T" + juce::String (maxT);
            }
            else
            {
                int minC = std::min (visSel.startClip, visSel.endClip) + 1;
                int maxC = std::max (visSel.startClip, visSel.endClip) + 1;
                breadcrumb = "> T" + juce::String (minT) + "-T" + juce::String (maxT)
                           + " > C" + juce::String (minC) + "-C" + juce::String (maxC);
            }
        }
        else if (panel == VimContext::Editor)
        {
            breadcrumb = "> " + trackInfo + " > C"
                       + juce::String (context.getSelectedClipIndex() + 1);
        }
        else if (panel == VimContext::Mixer && ! context.isMasterStripSelected())
        {
            auto focusName = context.getMixerFocusName();
            breadcrumb = "> " + trackInfo;
            if (focusName.isNotEmpty())
                breadcrumb += " > " + focusName;

            if (context.getMixerFocus() == VimContext::FocusPlugins)
            {
                int slot = context.getSelectedPluginSlot();
                if (slot < track.getNumPlugins())
                {
                    auto pluginState = track.getPlugin (slot);
                    breadcrumb += " > " + pluginState.getProperty (IDs::pluginName, "Plugin").toString();
                }
                else
                {
                    breadcrumb += " > [+]";
                }
            }
        }
        else if (panel == VimContext::Sequencer)
        {
            breadcrumb = "> R" + juce::String (context.getSeqRow() + 1)
                       + " > S" + juce::String (context.getSeqStep() + 1);
        }
    }
    else if (context.getPanel() == VimContext::Mixer && context.isMasterStripSelected())
    {
        auto focusName = context.getMixerFocusName();
        breadcrumb = "> Master";
        if (focusName.isNotEmpty())
            breadcrumb += " > " + focusName;

        if (context.getMixerFocus() == VimContext::FocusPlugins)
        {
            auto masterBus = arrangement.getProject().getState().getChildWithName (IDs::MASTER_BUS);
            auto chain = masterBus.isValid() ? masterBus.getChildWithName (IDs::PLUGIN_CHAIN) : juce::ValueTree();
            int numPlugins = chain.isValid() ? chain.getNumChildren() : 0;
            int slot = context.getSelectedPluginSlot();

            if (slot < numPlugins)
            {
                auto pluginState = chain.getChild (slot);
                breadcrumb += " > " + pluginState.getProperty (IDs::pluginName, "Plugin").toString();
            }
            else
            {
                breadcrumb += " > [+]";
            }
        }
    }
    else
    {
        breadcrumb = "No track selected";
    }

    g.setColour (juce::Colour (0xffa6adc8));
    g.drawText (breadcrumb, cursorArea.reduced (6, 0),
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
