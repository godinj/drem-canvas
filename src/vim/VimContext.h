#pragma once
#include <JuceHeader.h>

namespace dc
{

class VimContext
{
public:
    enum Panel { Editor, Mixer, Sequencer };
    enum MixerFocus { FocusNone, FocusVolume, FocusPan, FocusPlugins };

    VimContext() = default;

    // Panel
    Panel getPanel() const { return activePanel; }
    void cyclePanel();
    juce::String getPanelName() const;

    // Mixer parameter focus
    MixerFocus getMixerFocus() const { return mixerFocus; }
    void setMixerFocus (MixerFocus focus) { mixerFocus = focus; }
    juce::String getMixerFocusName() const;

    // Clip selection
    int getSelectedClipIndex() const { return selectedClipIndex; }
    void setSelectedClipIndex (int index) { selectedClipIndex = index; }

    // Sequencer cursor
    int getSeqRow() const  { return seqRow; }
    int getSeqStep() const { return seqStep; }
    void setSeqRow (int r)  { seqRow = r; }
    void setSeqStep (int s) { seqStep = s; }

    // Clipboard (single-clip — legacy, also set from first item of multi)
    void setClipboard (const juce::ValueTree& clip) { clipboard = clip.createCopy(); }
    juce::ValueTree getClipboard() const { return clipboard; }
    bool hasClipboardContent() const { return clipboard.isValid(); }

    // Multi-clip clipboard (for operator yank/delete)
    void setClipboardMulti (const juce::Array<juce::ValueTree>& clips, bool linewise);
    const juce::Array<juce::ValueTree>& getClipboardMulti() const { return clipboardMulti; }
    bool isClipboardLinewise() const { return clipboardLinewise; }

private:
    Panel activePanel = Editor;
    MixerFocus mixerFocus = FocusNone;
    int selectedClipIndex = 0;
    int seqRow  = 0;
    int seqStep = 0;
    juce::ValueTree clipboard;
    juce::Array<juce::ValueTree> clipboardMulti;
    bool clipboardLinewise = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimContext)
};

} // namespace dc
