#pragma once
#include <JuceHeader.h>

namespace dc
{

class VimContext
{
public:
    enum Panel { Editor, Mixer };

    VimContext() = default;

    // Panel
    Panel getPanel() const { return activePanel; }
    void cyclePanel();
    juce::String getPanelName() const;

    // Clip selection
    int getSelectedClipIndex() const { return selectedClipIndex; }
    void setSelectedClipIndex (int index) { selectedClipIndex = index; }

    // Clipboard
    void setClipboard (const juce::ValueTree& clip) { clipboard = clip.createCopy(); }
    juce::ValueTree getClipboard() const { return clipboard; }
    bool hasClipboardContent() const { return clipboard.isValid(); }

private:
    Panel activePanel = Editor;
    int selectedClipIndex = 0;
    juce::ValueTree clipboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimContext)
};

} // namespace dc
