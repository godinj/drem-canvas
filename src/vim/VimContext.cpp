#include "VimContext.h"

namespace dc
{

void VimContext::cyclePanel()
{
    switch (activePanel)
    {
        case Editor:    activePanel = Mixer;     break;
        case Mixer:     activePanel = Sequencer; break;
        case Sequencer: activePanel = Editor;    break;
    }
}

juce::String VimContext::getPanelName() const
{
    switch (activePanel)
    {
        case Editor:    return "Editor";
        case Mixer:     return "Mixer";
        case Sequencer: return "Sequencer";
    }
    return "Editor";
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
