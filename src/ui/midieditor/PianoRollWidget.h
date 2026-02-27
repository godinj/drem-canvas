#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ScrollViewWidget.h"
#include "PianoKeyboardWidget.h"
#include "NoteGridWidget.h"
#include "PianoRollRulerWidget.h"
#include "VelocityLaneWidget.h"
#include "CCLaneWidget.h"
#include "NoteWidget.h"
#include "model/Project.h"
#include "model/MidiClip.h"
#include "engine/TransportController.h"
#include <JuceHeader.h>
#include <vector>
#include <set>
#include <memory>

namespace dc
{
namespace ui
{

class PianoRollWidget : public gfx::Widget
{
public:
    PianoRollWidget (Project& project, TransportController& transport);

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void resized() override;
    bool mouseWheel (const gfx::WheelEvent& e) override;

    void loadClip (const juce::ValueTree& midiClipState);

    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    // Tool modes
    enum Tool { Select, Draw, Erase };
    void setTool (Tool t);

    Tool getTool() const { return currentTool; }

    // Snap
    void setSnapEnabled (bool enabled) { snapEnabled = enabled; }
    bool isSnapEnabled() const { return snapEnabled; }
    void setGridDivision (int div);
    int getGridDivision() const { return gridDivision; }
    double snapBeat (double beat) const;

    // Selection
    void selectNote (int index, bool addToSelection = false);
    void deselectAll();
    void selectAll();
    void selectNotesInRect (float x, float y, float w, float h);
    const std::set<int>& getSelectedNoteIndices() const { return selectedNoteIndices; }

    // Editing operations
    void deleteSelectedNotes (char reg = '\0');
    void copySelectedNotes (char reg = '\0');
    void cutSelectedNotes (char reg = '\0');
    void pasteNotes (char reg = '\0');
    void duplicateSelectedNotes();
    void transposeSelected (int semitones);
    void quantizeSelected (double strength = 1.0);
    void humanizeSelected (double timingRange = 0.1, double velocityRange = 10.0);

    // Zoom
    void zoomHorizontal (float factor);
    void zoomVertical (float factor);
    void zoomToFit();

    // Playhead
    void setPlayheadBeat (double beat) { playheadBeat = beat; }

    // Vim cursor
    int getPrBeatCol() const { return prBeatCol; }
    int getPrNoteRow() const { return prNoteRow; }
    void setPrBeatCol (int col) { prBeatCol = col; ensureCursorVisible(); repaint(); }
    void setPrNoteRow (int row) { prNoteRow = row; ensureCursorVisible(); repaint(); }

    // Animation
    void animationTick (double timestampMs) override;

    // Recording mode
    enum RecordMode { Overdub, Replace };
    void setRecordMode (RecordMode mode) { recordMode = mode; }

    // Velocity lane toggle
    void setVelocityLaneVisible (bool visible);
    bool isVelocityLaneVisible() const { return velocityLaneVisible; }

    // CC lane toggle
    void setCCLaneVisible (bool visible);
    bool isCCLaneVisible() const { return ccLaneVisible; }

private:
    void rebuildNotes();
    void ensureCursorVisible();

    Project& project;
    TransportController& transportController;

    juce::ValueTree clipState;
    PianoKeyboardWidget keyboard;
    PianoRollRulerWidget ruler;
    gfx::ScrollViewWidget scrollView;
    NoteGridWidget noteGrid;
    VelocityLaneWidget velocityLane;
    CCLaneWidget ccLane;
    std::vector<std::unique_ptr<NoteWidget>> noteWidgets;

    // Tools
    Tool currentTool = Select;
    bool snapEnabled = true;
    int gridDivision = 4;

    // Selection
    std::set<int> selectedNoteIndices;
    bool rubberBanding = false;
    float rubberBandStartX = 0.0f;
    float rubberBandStartY = 0.0f;
    float rubberBandEndX = 0.0f;
    float rubberBandEndY = 0.0f;

    // Grid/zoom
    float pixelsPerBeat = 80.0f;
    float rowHeight = 12.0f;
    static constexpr float keyboardWidth = 60.0f;

    // Playhead
    double playheadBeat = -1.0;

    // Vim cursor
    int prBeatCol = 0;
    int prNoteRow = 60;

    // Trim offset (for split clips)
    double trimOffsetBeats = 0.0;

    // Velocity / CC lane state
    bool velocityLaneVisible = false;
    bool ccLaneVisible = false;
    static constexpr float velocityLaneHeight = 60.0f;
    static constexpr float ccLaneHeight = 80.0f;

    // Recording
    RecordMode recordMode = Overdub;
};

} // namespace ui
} // namespace dc
