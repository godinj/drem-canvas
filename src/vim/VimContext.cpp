#include "VimContext.h"

namespace dc
{

void VimContext::setPanel (Panel p)
{
    activePanel = p;

    if (activePanel == Mixer)
        mixerFocus = FocusVolume;
    else
        mixerFocus = FocusNone;
}

void VimContext::cyclePanel()
{
    // PianoRoll is entered explicitly via Enter, not cycled to
    switch (activePanel)
    {
        case Editor:    activePanel = Mixer;     break;
        case Mixer:     activePanel = Sequencer; break;
        case Sequencer: activePanel = Editor;    break;
        case PianoRoll: activePanel = Editor;    break;
    }

    // Default to FocusVolume when entering Mixer, clear otherwise
    if (activePanel == Mixer)
        mixerFocus = FocusVolume;
    else
        mixerFocus = FocusNone;
}

juce::String VimContext::getPanelName() const
{
    switch (activePanel)
    {
        case Editor:    return "Editor";
        case Mixer:     return "Mixer";
        case Sequencer: return "Sequencer";
        case PianoRoll: return "PianoRoll";
    }
    return "Editor";
}

juce::String VimContext::getMixerFocusName() const
{
    switch (mixerFocus)
    {
        case FocusVolume:  return "Volume";
        case FocusPan:     return "Pan";
        case FocusPlugins: return "Plugins";
        case FocusNone:    return "";
    }
    return "";
}

void VimContext::setVisualSelection (const VisualSelection& sel)
{
    visualSelection = sel;
}

void VimContext::clearVisualSelection()
{
    visualSelection = VisualSelection();
}

bool VimContext::isTrackInVisualSelection (int trackIndex) const
{
    if (! visualSelection.active)
        return false;

    int minTrack = std::min (visualSelection.startTrack, visualSelection.endTrack);
    int maxTrack = std::max (visualSelection.startTrack, visualSelection.endTrack);
    return trackIndex >= minTrack && trackIndex <= maxTrack;
}

bool VimContext::isClipInVisualSelection (int trackIndex, int clipIndex) const
{
    if (! visualSelection.active)
        return false;

    if (! isTrackInVisualSelection (trackIndex))
        return false;

    // Linewise — all clips on selected tracks
    if (visualSelection.linewise)
        return true;

    int minTrack = std::min (visualSelection.startTrack, visualSelection.endTrack);
    int maxTrack = std::max (visualSelection.startTrack, visualSelection.endTrack);

    if (minTrack == maxTrack)
    {
        // Single track — clip range
        int minClip = std::min (visualSelection.startClip, visualSelection.endClip);
        int maxClip = std::max (visualSelection.startClip, visualSelection.endClip);
        return clipIndex >= minClip && clipIndex <= maxClip;
    }

    // Multi-track clipwise: boundary tracks have partial ranges, intermediate tracks select all
    if (trackIndex > minTrack && trackIndex < maxTrack)
        return true; // intermediate track — all clips

    // Determine which end is start vs end based on original order
    bool startIsMin = (visualSelection.startTrack <= visualSelection.endTrack);
    int anchorClip = startIsMin ? visualSelection.startClip : visualSelection.endClip;
    int cursorClip = startIsMin ? visualSelection.endClip : visualSelection.startClip;

    if (trackIndex == minTrack)
        return clipIndex >= anchorClip;

    // trackIndex == maxTrack
    return clipIndex <= cursorClip;
}

} // namespace dc
