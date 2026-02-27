#pragma once
#include <JuceHeader.h>

namespace dc
{

class VimContext
{
public:
    enum Panel { Editor, Mixer, Sequencer, PianoRoll };
    enum MixerFocus { FocusNone, FocusVolume, FocusPan, FocusPlugins };

    VimContext() = default;

    // Panel
    Panel getPanel() const { return activePanel; }
    void setPanel (Panel p);
    void cyclePanel();
    juce::String getPanelName() const;

    // State for the currently-open clip in the piano roll
    juce::ValueTree openClipState;

    // Mixer parameter focus
    MixerFocus getMixerFocus() const { return mixerFocus; }
    void setMixerFocus (MixerFocus focus) { mixerFocus = focus; }
    juce::String getMixerFocusName() const;

    // Visual selection
    struct VisualSelection
    {
        bool active = false;
        bool linewise = false;
        int startTrack = 0;
        int startClip = 0;
        int endTrack = 0;
        int endClip = 0;
    };

    void setVisualSelection (const VisualSelection& sel);
    const VisualSelection& getVisualSelection() const { return visualSelection; }
    void clearVisualSelection();
    bool isClipInVisualSelection (int trackIndex, int clipIndex) const;
    bool isTrackInVisualSelection (int trackIndex) const;

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
    VisualSelection visualSelection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimContext)
};

} // namespace dc
