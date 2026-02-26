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

void VimContext::setClipboardMulti (const juce::Array<juce::ValueTree>& clips, bool linewise)
{
    clipboardMulti.clear();
    for (auto& c : clips)
        clipboardMulti.add (c.createCopy());

    clipboardLinewise = linewise;

    // Keep single-clip clipboard in sync
    if (clips.size() > 0)
        clipboard = clips[0].createCopy();
}

} // namespace dc
