#include "VimStatusBar.h"
#include "model/Track.h"
#include "gui/common/ColourBridge.h"
#include "dc/foundation/types.h"
#include <string>

using dc::bridge::toJuce;

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
    g.fillAll (toJuce (0xff181825));

    auto font = juce::Font (juce::FontOptions (14.0f));
    g.setFont (font);

    // ── Command mode — full-width command line ────────────────────────
    if (engine.getMode() == VimEngine::Command)
    {
        g.setColour (toJuce (0xffcdd6f4));
        g.drawText (":" + engine.getCommandBuffer(), area.reduced (6, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // ── Plugin search — full-width search line ──────────────────────────
    if (engine.getMode() == VimEngine::PluginMenu && engine.isPluginSearchActive())
    {
        g.setColour (toJuce (0xffcdd6f4));
        g.drawText ("/" + engine.getPluginSearchBuffer(), area.reduced (6, 0),
                    juce::Justification::centredLeft);
        return;
    }

    // ── Mode segment ────────────────────────────────────────────────────
    auto modeArea = area.removeFromLeft (160);
    dc::Colour modeColour;
    const char* modeText;

    switch (engine.getMode())
    {
        case VimEngine::Normal:     modeColour = dc::Colour (0xff50c878); modeText = "-- NORMAL --"; break;
        case VimEngine::Insert:     modeColour = dc::Colour (0xff4a9eff); modeText = "-- INSERT --"; break;
        case VimEngine::PluginMenu: modeColour = dc::Colour (0xffcba6f7); modeText = "-- PLUGIN --"; break;
        case VimEngine::Visual:     modeColour = dc::Colour (0xffff9944); modeText = "-- VISUAL --"; break;
        case VimEngine::VisualLine: modeColour = dc::Colour (0xffff9944); modeText = "-- V-LINE --"; break;
        default:                    modeColour = dc::Colour (0xff50c878); modeText = "-- NORMAL --"; break;
    }

    g.setColour (toJuce (modeColour));
    g.fillRect (modeArea);

    g.setColour (toJuce (0xff181825));
    g.drawText (modeText, modeArea.reduced (6, 0), juce::Justification::centredLeft);

    // ── Pending state indicator ─────────────────────────────────────────
    if (engine.hasPendingState())
    {
        auto pendingArea = area.removeFromLeft (80);
        g.setColour (toJuce (0xffffcc00)); // yellow
        g.drawText (engine.getPendingDisplay(), pendingArea.reduced (4, 0),
                    juce::Justification::centredLeft);
    }

    // ── Context panel segment (prominent green on dark bg) ─────────────
    auto panelArea = area.removeFromLeft (120);
    g.setColour (toJuce (0xff202030));
    g.fillRect (panelArea);
    g.setColour (toJuce (0xff50c878));
    g.drawText (context.getPanelName(), panelArea.reduced (6, 0),
                juce::Justification::centredLeft);

    // ── Breadcrumb info segment (context-dependent) ─────────────────────
    auto cursorArea = area.removeFromLeft (280);
    int trackIdx = arrangement.getSelectedTrackIndex();
    std::string breadcrumb;

    if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
    {
        Track track = arrangement.getTrack (trackIdx);
        std::string trackInfo = "T" + std::to_string (trackIdx + 1) + ":"
                               + track.getName();

        auto panel = context.getPanel();
        auto& visSel = context.getVisualSelection();
        if (panel == VimContext::Editor && visSel.active)
        {
            int minT = std::min (visSel.startTrack, visSel.endTrack) + 1;
            int maxT = std::max (visSel.startTrack, visSel.endTrack) + 1;

            if (visSel.linewise)
            {
                breadcrumb = "> T" + std::to_string (minT) + "-T" + std::to_string (maxT);
            }
            else
            {
                int minC = std::min (visSel.startClip, visSel.endClip) + 1;
                int maxC = std::max (visSel.startClip, visSel.endClip) + 1;
                breadcrumb = "> T" + std::to_string (minT) + "-T" + std::to_string (maxT)
                           + " > C" + std::to_string (minC) + "-C" + std::to_string (maxC);
            }
        }
        else if (panel == VimContext::Editor)
        {
            breadcrumb = "> " + trackInfo + " > C"
                       + std::to_string (context.getSelectedClipIndex() + 1);
        }
        else if (panel == VimContext::Mixer && ! context.isMasterStripSelected())
        {
            auto focusName = context.getMixerFocusName();
            breadcrumb = "> " + trackInfo;
            if (! focusName.empty())
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
            breadcrumb = "> R" + std::to_string (context.getSeqRow() + 1)
                       + " > S" + std::to_string (context.getSeqStep() + 1);
        }
    }
    else if (context.getPanel() == VimContext::Mixer && context.isMasterStripSelected())
    {
        auto focusName = context.getMixerFocusName();
        breadcrumb = "> Master";
        if (! focusName.empty())
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

    g.setColour (toJuce (0xffa6adc8));
    g.drawText (breadcrumb, cursorArea.reduced (6, 0),
                juce::Justification::centredLeft);

    // ── Playhead info (right-aligned) ───────────────────────────────────
    auto playheadArea = area;
    g.setColour (toJuce (0xffa6adc8));
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
