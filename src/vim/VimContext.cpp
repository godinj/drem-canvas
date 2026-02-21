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

} // namespace dc
