#include "VimContext.h"

namespace dc
{

void VimContext::cyclePanel()
{
    activePanel = (activePanel == Editor) ? Mixer : Editor;
}

juce::String VimContext::getPanelName() const
{
    return activePanel == Editor ? "Editor" : "Mixer";
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
