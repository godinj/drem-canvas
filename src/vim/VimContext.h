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
    void setMixerFocus (MixerFocus focus);
    juce::String getMixerFocusName() const;

    // Plugin slot selection (within Plugins focus)
    int getSelectedPluginSlot() const { return selectedPluginSlot; }
    void setSelectedPluginSlot (int slot) { selectedPluginSlot = slot; }

    // Master strip selection (separate from track index)
    bool isMasterStripSelected() const { return masterStripSelected; }
    void setMasterStripSelected (bool selected) { masterStripSelected = selected; }

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

    // Grid cursor position (in samples, always grid-snapped)
    int64_t getGridCursorPosition() const { return gridCursorPosition; }
    void setGridCursorPosition (int64_t pos) { gridCursorPosition = pos; }

    // Grid visual selection (for grid-based visual mode)
    struct GridVisualSelection
    {
        bool active = false;
        bool linewise = false;
        int startTrack = 0;
        int endTrack = 0;
        int64_t startPos = 0; // grid position in samples
        int64_t endPos = 0;   // grid position in samples
    };

    void setGridVisualSelection (const GridVisualSelection& sel) { gridVisualSelection = sel; }
    const GridVisualSelection& getGridVisualSelection() const { return gridVisualSelection; }
    void clearGridVisualSelection() { gridVisualSelection = GridVisualSelection(); }

    // Clip selection (derived from grid cursor position)
    int getSelectedClipIndex() const { return selectedClipIndex; }
    void setSelectedClipIndex (int index) { selectedClipIndex = index; }

    // Sequencer cursor
    int getSeqRow() const  { return seqRow; }
    int getSeqStep() const { return seqStep; }
    void setSeqRow (int r)  { seqRow = r; }
    void setSeqStep (int s) { seqStep = s; }

private:
    Panel activePanel = Editor;
    MixerFocus mixerFocus = FocusNone;
    int selectedClipIndex = 0;
    int selectedPluginSlot = 0;
    bool masterStripSelected = false;
    int64_t gridCursorPosition = 0;
    int seqRow  = 0;
    int seqStep = 0;
    VisualSelection visualSelection;
    GridVisualSelection gridVisualSelection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimContext)
};

} // namespace dc
