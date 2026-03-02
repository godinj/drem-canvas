#include "PianoRollWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "utils/UndoSystem.h"
#include <algorithm>
#include <random>

namespace dc
{
namespace ui
{

PianoRollWidget::PianoRollWidget (Project& p, TransportController& t)
    : project (p), transportController (t), velocityLane (p), ccLane (p)
{
    addChild (&ruler);
    addChild (&keyboard);
    addChild (&scrollView);
    addChild (&velocityLane);
    addChild (&ccLane);
    scrollView.setContentWidget (&noteGrid);

    velocityLane.setVisible (false);
    ccLane.setVisible (false);

    // Wire ruler seek: click on ruler to seek transport
    ruler.onSeek = [this] (double beat)
    {
        if (! clipState.isValid())
            return;

        double sr = project.getSampleRate();
        double tempo = project.getTempo();
        int64_t clipStart = clipState.getProperty (IDs::startPosition).getIntOr (0);

        int64_t samplePos = clipStart + static_cast<int64_t> (beat * 60.0 / tempo * sr);
        if (samplePos < 0) samplePos = 0;
        transportController.setPositionInSamples (samplePos);
    };

    // Wire grid callbacks for draw/erase/selection
    noteGrid.onDrawNote = [this] (int noteNumber, double beat)
    {
        if (! clipState.isValid())
            return;

        double snappedBeat = snapBeat (beat) + trimOffsetBeats;
        double defaultLength = 1.0 / gridDivision;

        ScopedTransaction txn (project.getUndoSystem(), "Add Note");
        MidiClip clip (clipState);
        clip.addNote (noteNumber, snappedBeat, defaultLength, 100, &project.getUndoManager());
    };

    noteGrid.onEraseNote = [this] (int noteNumber, double beat)
    {
        if (! clipState.isValid())
            return;

        // Convert display beat back to stored beat
        double storedBeat = beat + trimOffsetBeats;

        // Find note at this position
        for (int i = 0; i < clipState.getNumChildren(); ++i)
        {
            auto child = clipState.getChild (i);
            if (child.getType() != IDs::NOTE)
                continue;

            int nn = static_cast<int> (child.getProperty (IDs::noteNumber).getIntOr (60));
            double sb = child.getProperty (IDs::startBeat).getDoubleOr (0.0);
            double lb = child.getProperty (IDs::lengthBeats).getDoubleOr (0.25);

            if (nn == noteNumber && storedBeat >= sb && storedBeat < sb + lb)
            {
                ScopedTransaction txn (project.getUndoSystem(), "Erase Note");
                MidiClip clip (clipState);
                clip.removeNote (i, &project.getUndoManager());
                return;
            }
        }
    };

    noteGrid.onRubberBandSelect = [this] (float x, float y, float w, float h)
    {
        selectNotesInRect (x, y, w, h);
    };

    noteGrid.onEmptyClick = [this]()
    {
        deselectAll();
    };

    setAnimating (true);
}

void PianoRollWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void PianoRollWidget::paintOverChildren (gfx::Canvas& canvas)
{
    using namespace gfx;

    if (! clipState.isValid())
        return;

    // Draw playhead
    if (playheadBeat >= 0.0)
    {
        float phX = noteGrid.beatsToX (playheadBeat) - scrollView.getScrollOffsetX() + keyboardWidth;
        float rulerH = PianoRollRulerWidget::rulerHeight;
        if (phX >= keyboardWidth && phX <= getWidth())
        {
            canvas.fillRect (Rect (phX, rulerH, 1.5f, getHeight() - rulerH),
                             Color (255, 60, 60, 200));
        }
    }

    // Draw vim cursor highlight
    float cursorX = noteGrid.beatsToX (static_cast<double> (prBeatCol) / gridDivision)
                    - scrollView.getScrollOffsetX() + keyboardWidth;
    float cursorY = noteGrid.noteToY (prNoteRow) - scrollView.getScrollOffsetY()
                    + PianoRollRulerWidget::rulerHeight;
    float cursorW = pixelsPerBeat / static_cast<float> (gridDivision);

    canvas.strokeRect (Rect (cursorX, cursorY, cursorW, rowHeight),
                       Color (255, 200, 50, 150), 1.5f);

    // Draw rubber band selection rect
    if (rubberBanding)
    {
        float rx = std::min (rubberBandStartX, rubberBandEndX);
        float ry = std::min (rubberBandStartY, rubberBandEndY);
        float rw = std::abs (rubberBandEndX - rubberBandStartX);
        float rh = std::abs (rubberBandEndY - rubberBandStartY);

        canvas.fillRect (Rect (rx, ry, rw, rh), Color (100, 150, 255, 40));
        canvas.strokeRect (Rect (rx, ry, rw, rh), Color (100, 150, 255, 150), 1.0f);
    }
}

void PianoRollWidget::resized()
{
    float w = getWidth();
    float h = getHeight();
    float rulerH = PianoRollRulerWidget::rulerHeight;

    // Compute bottom lanes height
    float bottomLaneH = 0.0f;
    if (velocityLaneVisible) bottomLaneH += velocityLaneHeight;
    if (ccLaneVisible) bottomLaneH += ccLaneHeight;

    float scrollAreaH = h - rulerH - bottomLaneH;

    // Ruler at top (after keyboard width offset)
    ruler.setBounds (keyboardWidth, 0, w - keyboardWidth, rulerH);
    ruler.setPixelsPerBeat (pixelsPerBeat);

    // Keyboard on the left (below ruler)
    keyboard.setBounds (0, rulerH, keyboardWidth, scrollAreaH);

    // ScrollView for note grid
    scrollView.setBounds (keyboardWidth, rulerH, w - keyboardWidth, scrollAreaH);

    float contentWidth = pixelsPerBeat * 128.0f; // 128 beats (32 bars at 4/4)
    float contentHeight = 128.0f * rowHeight;
    scrollView.setContentSize (contentWidth, contentHeight);

    noteGrid.setBounds (0, 0, contentWidth, contentHeight);
    noteGrid.setPixelsPerBeat (pixelsPerBeat);
    noteGrid.setRowHeight (rowHeight);
    noteGrid.setGridDivision (gridDivision);
    noteGrid.setTempo (project.getTempo());

    // Velocity lane below scroll view
    float currentLaneY = rulerH + scrollAreaH;

    if (velocityLaneVisible)
    {
        velocityLane.setVisible (true);
        velocityLane.setBounds (keyboardWidth, currentLaneY, w - keyboardWidth, velocityLaneHeight);
        velocityLane.setClipState (clipState);
        velocityLane.setPixelsPerBeat (pixelsPerBeat);
        velocityLane.setScrollOffset (scrollView.getScrollOffsetX());
        velocityLane.setSelectedNotes (&selectedNoteIndices);
        currentLaneY += velocityLaneHeight;
    }
    else
    {
        velocityLane.setVisible (false);
    }

    // CC lane below velocity lane
    if (ccLaneVisible)
    {
        ccLane.setVisible (true);
        ccLane.setBounds (keyboardWidth, currentLaneY, w - keyboardWidth, ccLaneHeight);
        ccLane.setClipState (clipState);
        ccLane.setPixelsPerBeat (pixelsPerBeat);
        ccLane.setScrollOffset (scrollView.getScrollOffsetX());
    }
    else
    {
        ccLane.setVisible (false);
    }

    // Sync keyboard scroll with grid scroll
    keyboard.setScrollOffset (scrollView.getScrollOffsetY());

    rebuildNotes();
}

void PianoRollWidget::loadClip (const PropertyTree& state)
{
    if (clipState.isValid())
        clipState.removeListener (this);

    clipState = state;
    selectedNoteIndices.clear();

    if (clipState.isValid())
    {
        clipState.addListener (this);

        // Compute beat offset so ruler bar numbers match the arrangement
        int64_t clipStart = clipState.getProperty (IDs::startPosition).getIntOr (0);
        int64_t trimStart = clipState.getProperty (IDs::trimStart).getIntOr (0);
        double sr = project.getSampleRate();
        double tempo = project.getTempo();

        trimOffsetBeats = (sr > 0.0 && tempo > 0.0)
            ? (static_cast<double> (trimStart) / sr) * tempo / 60.0
            : 0.0;

        double clipStartBeats = (static_cast<double> (clipStart) / sr) * tempo / 60.0;
        ruler.setBeatOffset (clipStartBeats + trimOffsetBeats);
    }
    else
    {
        trimOffsetBeats = 0.0;
        ruler.setBeatOffset (0.0);
    }

    rebuildNotes();
}

void PianoRollWidget::rebuildNotes()
{
    for (auto& nw : noteWidgets)
        noteGrid.removeChild (nw.get());
    noteWidgets.clear();

    if (! clipState.isValid())
        return;

    // Compute visible beat range from clip length
    double sr = project.getSampleRate();
    double tempo = project.getTempo();
    int64_t clipLength = clipState.getProperty (IDs::length).getIntOr (0);
    double clipLengthBeats = (sr > 0.0 && tempo > 0.0)
        ? (static_cast<double> (clipLength) / sr) * tempo / 60.0
        : 1e12;

    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto note = clipState.getChild (i);
        if (note.getType() != IDs::NOTE)
            continue;

        int noteNum = static_cast<int> (note.getProperty (IDs::noteNumber).getIntOr (60));
        auto startBeat = note.getProperty (IDs::startBeat).getDoubleOr (0.0) - trimOffsetBeats;
        auto lengthBeats = note.getProperty (IDs::lengthBeats).getDoubleOr (0.25);
        int velocity = static_cast<int> (note.getProperty (IDs::velocity).getIntOr (100));

        // Skip notes outside the visible clip region
        if (startBeat + lengthBeats <= 0.0 || startBeat >= clipLengthBeats)
            continue;

        // Clamp note to visible region
        if (startBeat < 0.0)
        {
            lengthBeats += startBeat;
            startBeat = 0.0;
        }

        float x = noteGrid.beatsToX (startBeat);
        float y = noteGrid.noteToY (noteNum);
        float w = static_cast<float> (lengthBeats * pixelsPerBeat);

        auto nw = std::make_unique<NoteWidget>();
        nw->setNoteNumber (noteNum);
        nw->setVelocity (velocity);
        nw->setSelected (selectedNoteIndices.count (i) > 0);
        nw->setBounds (x, y, w, rowHeight - 1.0f);

        // Wire drag callback — move note
        int noteIndex = i;
        nw->onDrag = [this, noteIndex] (float dx, float dy)
        {
            if (noteIndex >= clipState.getNumChildren())
                return;

            auto noteState = clipState.getChild (noteIndex);
            if (noteState.getType() != IDs::NOTE)
                return;

            project.getUndoSystem().beginCoalescedTransaction ("Move Note");
            auto& um = project.getUndoManager();

            double curBeat = noteState.getProperty (IDs::startBeat).getDoubleOr (0.0);
            int curNote = static_cast<int> (noteState.getProperty (IDs::noteNumber).getIntOr (60));

            double newBeat = curBeat + static_cast<double> (dx) / pixelsPerBeat;
            int newNote = curNote - static_cast<int> (dy / rowHeight);

            if (snapEnabled)
                newBeat = snapBeat (newBeat);

            newBeat = std::max (0.0, newBeat);
            newNote = std::clamp (newNote, 0, 127);

            noteState.setProperty (IDs::startBeat, Variant (newBeat), &um);
            noteState.setProperty (IDs::noteNumber, Variant (newNote), &um);

            MidiClip clip (clipState);
            clip.collapseChildrenToMidiData (&um);
        };

        // Wire resize callback — change note length
        nw->onResize = [this, noteIndex] (float newWidth)
        {
            if (noteIndex >= clipState.getNumChildren())
                return;

            auto noteState = clipState.getChild (noteIndex);
            if (noteState.getType() != IDs::NOTE)
                return;

            project.getUndoSystem().beginCoalescedTransaction ("Resize Note");
            auto& um = project.getUndoManager();

            double newLengthBeats = static_cast<double> (newWidth) / pixelsPerBeat;
            double minLength = 1.0 / (gridDivision * 4); // 1/16 beat minimum

            if (newLengthBeats < minLength)
                newLengthBeats = minLength;

            if (snapEnabled)
                newLengthBeats = snapBeat (newLengthBeats);

            noteState.setProperty (IDs::lengthBeats, Variant (newLengthBeats), &um);

            MidiClip clip (clipState);
            clip.collapseChildrenToMidiData (&um);
        };

        // Wire click callback — selection
        nw->onClicked = [this, noteIndex] (bool shiftHeld)
        {
            selectNote (noteIndex, shiftHeld);
        };

        noteGrid.addChild (nw.get());
        noteWidgets.push_back (std::move (nw));
    }
}

double PianoRollWidget::snapBeat (double beat) const
{
    if (! snapEnabled)
        return beat;

    double gridSize = 1.0 / static_cast<double> (gridDivision);
    return std::round (beat / gridSize) * gridSize;
}

void PianoRollWidget::setTool (Tool t)
{
    currentTool = t;

    switch (t)
    {
        case Select: noteGrid.setToolMode (NoteGridWidget::SelectTool); break;
        case Draw:   noteGrid.setToolMode (NoteGridWidget::DrawTool);   break;
        case Erase:  noteGrid.setToolMode (NoteGridWidget::EraseTool);  break;
    }

    repaint();
}

void PianoRollWidget::setGridDivision (int div)
{
    gridDivision = std::clamp (div, 1, 16);
    noteGrid.setGridDivision (gridDivision);
    repaint();
}

// ── Selection ────────────────────────────────────────────────────────────────

void PianoRollWidget::selectNote (int index, bool addToSelection)
{
    if (! addToSelection)
        selectedNoteIndices.clear();

    if (index >= 0)
    {
        if (selectedNoteIndices.count (index) > 0 && addToSelection)
            selectedNoteIndices.erase (index);
        else
            selectedNoteIndices.insert (index);
    }

    // Update visual state
    int noteIdx = 0;
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        if (clipState.getChild (i).getType() != IDs::NOTE)
            continue;

        if (noteIdx < static_cast<int> (noteWidgets.size()))
            noteWidgets[static_cast<size_t> (noteIdx)]->setSelected (
                selectedNoteIndices.count (i) > 0);
        ++noteIdx;
    }

    repaint();
}

void PianoRollWidget::deselectAll()
{
    selectedNoteIndices.clear();
    for (auto& nw : noteWidgets)
        nw->setSelected (false);
    repaint();
}

void PianoRollWidget::selectAll()
{
    selectedNoteIndices.clear();
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        if (clipState.getChild (i).getType() == IDs::NOTE)
            selectedNoteIndices.insert (i);
    }

    for (auto& nw : noteWidgets)
        nw->setSelected (true);
    repaint();
}

void PianoRollWidget::selectNotesInRect (float x, float y, float w, float h)
{
    selectedNoteIndices.clear();

    int noteIdx = 0;
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto child = clipState.getChild (i);
        if (child.getType() != IDs::NOTE)
            continue;

        if (noteIdx < static_cast<int> (noteWidgets.size()))
        {
            auto& nw = noteWidgets[static_cast<size_t> (noteIdx)];
            auto nb = nw->getBounds();

            // Check intersection
            if (nb.x < x + w && nb.x + nb.width > x
                && nb.y < y + h && nb.y + nb.height > y)
            {
                selectedNoteIndices.insert (i);
                nw->setSelected (true);
            }
            else
            {
                nw->setSelected (false);
            }
        }
        ++noteIdx;
    }
    repaint();
}

// ── Editing operations ───────────────────────────────────────────────────────

void PianoRollWidget::deleteSelectedNotes (char reg)
{
    if (! clipState.isValid() || selectedNoteIndices.empty())
        return;

    // Store deleted notes (Vim delete → unnamed + "1-"9 history)
    {
        std::vector<Clipboard::NoteEntry> entries;
        double minBeat = 1e12;

        for (int idx : selectedNoteIndices)
        {
            if (idx < clipState.getNumChildren())
            {
                double sb = clipState.getChild (idx).getProperty (IDs::startBeat).getDoubleOr (0.0);
                minBeat = std::min (minBeat, sb);
            }
        }

        if (minBeat < 1e12)
        {
            for (int idx : selectedNoteIndices)
            {
                if (idx < clipState.getNumChildren())
                {
                    auto note = clipState.getChild (idx);
                    double sb = note.getProperty (IDs::startBeat).getDoubleOr (0.0);
                    entries.push_back ({ note, sb - minBeat });
                }
            }

            project.getClipboard().storeNotes (reg, entries, false);
        }
    }

    ScopedTransaction txn (project.getUndoSystem(), "Delete Notes");
    auto& um = project.getUndoManager();

    // Remove in reverse order to preserve indices
    std::vector<int> sorted (selectedNoteIndices.begin(), selectedNoteIndices.end());
    std::sort (sorted.rbegin(), sorted.rend());

    for (int idx : sorted)
    {
        if (idx < clipState.getNumChildren())
            clipState.removeChild (idx, &um);
    }

    selectedNoteIndices.clear();

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

void PianoRollWidget::copySelectedNotes (char reg)
{
    std::vector<Clipboard::NoteEntry> entries;
    double minBeat = 1e12;

    // First pass: find minimum startBeat
    for (int idx : selectedNoteIndices)
    {
        if (idx < clipState.getNumChildren())
        {
            double sb = clipState.getChild (idx).getProperty (IDs::startBeat).getDoubleOr (0.0);
            minBeat = std::min (minBeat, sb);
        }
    }

    if (minBeat >= 1e12)
        return;

    // Second pass: build entries with relative beat offsets
    for (int idx : selectedNoteIndices)
    {
        if (idx < clipState.getNumChildren())
        {
            auto note = clipState.getChild (idx);
            double sb = note.getProperty (IDs::startBeat).getDoubleOr (0.0);
            entries.push_back ({ note, sb - minBeat });
        }
    }

    project.getClipboard().storeNotes (reg, entries, true);
}

void PianoRollWidget::cutSelectedNotes (char reg)
{
    // deleteSelectedNotes yanks before deleting (Vim semantics)
    deleteSelectedNotes (reg);
}

void PianoRollWidget::pasteNotes (char reg)
{
    auto& regEntry = project.getClipboard().get (reg);
    if (! regEntry.hasNotes() || ! clipState.isValid())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Notes");
    auto& um = project.getUndoManager();

    // Convert display beat to stored beat (accounting for clip's trim offset)
    double cursorBeat = static_cast<double> (prBeatCol) / gridDivision + trimOffsetBeats;

    selectedNoteIndices.clear();

    for (auto& entry : regEntry.noteEntries)
    {
        auto newNote = entry.noteData.createDeepCopy();
        newNote.setProperty (IDs::startBeat, Variant (cursorBeat + entry.beatOffset), &um);
        clipState.addChild (std::move (newNote), -1, &um);
        selectedNoteIndices.insert (clipState.getNumChildren() - 1);
    }

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

void PianoRollWidget::duplicateSelectedNotes()
{
    if (selectedNoteIndices.empty() || ! clipState.isValid())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Duplicate Notes");
    auto& um = project.getUndoManager();

    // Find rightmost edge to place duplicates after
    double maxEnd = 0.0;
    std::vector<PropertyTree> copies;

    for (int idx : selectedNoteIndices)
    {
        if (idx < clipState.getNumChildren())
        {
            auto child = clipState.getChild (idx);
            double end = child.getProperty (IDs::startBeat).getDoubleOr (0.0)
                       + child.getProperty (IDs::lengthBeats).getDoubleOr (0.25);
            maxEnd = std::max (maxEnd, end);
            copies.push_back (child.createDeepCopy());
        }
    }

    // Find minimum start to calculate span
    double minStart = 1e12;
    for (auto& c : copies)
        minStart = std::min (minStart, c.getProperty (IDs::startBeat).getDoubleOr (0.0));

    double offset = maxEnd - minStart;

    selectedNoteIndices.clear();
    for (auto& c : copies)
    {
        double sb = c.getProperty (IDs::startBeat).getDoubleOr (0.0);
        c.setProperty (IDs::startBeat, Variant (sb + offset), &um);
        clipState.addChild (std::move (c), -1, &um);
        selectedNoteIndices.insert (clipState.getNumChildren() - 1);
    }

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

void PianoRollWidget::transposeSelected (int semitones)
{
    if (selectedNoteIndices.empty() || ! clipState.isValid())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Transpose Notes");
    auto& um = project.getUndoManager();

    for (int idx : selectedNoteIndices)
    {
        if (idx < clipState.getNumChildren())
        {
            auto child = clipState.getChild (idx);
            int noteNum = static_cast<int> (child.getProperty (IDs::noteNumber).getIntOr (60));
            noteNum = std::clamp (noteNum + semitones, 0, 127);
            child.setProperty (IDs::noteNumber, Variant (noteNum), &um);
        }
    }

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

void PianoRollWidget::quantizeSelected (double strength)
{
    if (selectedNoteIndices.empty() || ! clipState.isValid())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Quantize Notes");
    auto& um = project.getUndoManager();

    double gridSize = 1.0 / static_cast<double> (gridDivision);

    for (int idx : selectedNoteIndices)
    {
        if (idx < clipState.getNumChildren())
        {
            auto child = clipState.getChild (idx);
            double startBeat = child.getProperty (IDs::startBeat).getDoubleOr (0.0);
            double quantized = std::round (startBeat / gridSize) * gridSize;
            double newBeat = startBeat + (quantized - startBeat) * strength;
            child.setProperty (IDs::startBeat, Variant (newBeat), &um);
        }
    }

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

void PianoRollWidget::humanizeSelected (double timingRange, double velocityRange)
{
    if (selectedNoteIndices.empty() || ! clipState.isValid())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Humanize Notes");
    auto& um = project.getUndoManager();

    std::random_device rd;
    std::mt19937 gen (rd());
    std::uniform_real_distribution<double> timeDist (-timingRange, timingRange);
    std::uniform_real_distribution<double> velDist (-velocityRange, velocityRange);

    for (int idx : selectedNoteIndices)
    {
        if (idx < clipState.getNumChildren())
        {
            auto child = clipState.getChild (idx);

            double startBeat = child.getProperty (IDs::startBeat).getDoubleOr (0.0);
            startBeat = std::max (0.0, startBeat + timeDist (gen));
            child.setProperty (IDs::startBeat, Variant (startBeat), &um);

            int vel = static_cast<int> (child.getProperty (IDs::velocity).getIntOr (100));
            vel = std::clamp (vel + static_cast<int> (velDist (gen)), 1, 127);
            child.setProperty (IDs::velocity, Variant (vel), &um);
        }
    }

    MidiClip clip (clipState);
    clip.collapseChildrenToMidiData (&um);
}

// ── Zoom ─────────────────────────────────────────────────────────────────────

void PianoRollWidget::zoomHorizontal (float factor)
{
    pixelsPerBeat = std::clamp (pixelsPerBeat * factor, 10.0f, 400.0f);
    resized();
    repaint();
}

void PianoRollWidget::zoomVertical (float factor)
{
    rowHeight = std::clamp (rowHeight * factor, 4.0f, 30.0f);
    keyboard.setRowHeight (rowHeight);
    resized();
    repaint();
}

void PianoRollWidget::zoomToFit()
{
    if (! clipState.isValid())
        return;

    double maxBeat = 0.0;
    int minNote = 127, maxNote = 0;

    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto child = clipState.getChild (i);
        if (child.getType() != IDs::NOTE)
            continue;

        double endBeat = child.getProperty (IDs::startBeat).getDoubleOr (0.0)
                       + child.getProperty (IDs::lengthBeats).getDoubleOr (0.25);
        int noteNum = static_cast<int> (child.getProperty (IDs::noteNumber).getIntOr (60));

        maxBeat = std::max (maxBeat, endBeat);
        minNote = std::min (minNote, noteNum);
        maxNote = std::max (maxNote, noteNum);
    }

    if (maxBeat <= 0.0 || maxNote < minNote)
        return;

    float availableW = getWidth() - keyboardWidth;
    float availableH = getHeight() - PianoRollRulerWidget::rulerHeight;

    pixelsPerBeat = std::clamp (availableW / static_cast<float> (maxBeat + 1.0),
        10.0f, 400.0f);

    int noteRange = maxNote - minNote + 2;
    rowHeight = std::clamp (availableH / static_cast<float> (noteRange), 4.0f, 30.0f);

    keyboard.setRowHeight (rowHeight);
    resized();

    // Scroll to center on note range
    float scrollY = noteGrid.noteToY (maxNote + 1);
    scrollView.setScrollOffset (0, scrollY);
    keyboard.setScrollOffset (scrollY);
}

bool PianoRollWidget::mouseWheel (const gfx::WheelEvent& e)
{
    if (e.control || e.command)
    {
        // Ctrl+scroll = horizontal zoom
        float factor = e.deltaY > 0 ? 1.15f : 0.87f;
        zoomHorizontal (factor);
        return true;
    }
    else if (e.shift)
    {
        // Shift+scroll = vertical zoom
        float factor = e.deltaY > 0 ? 1.15f : 0.87f;
        zoomVertical (factor);
        return true;
    }
    return false;
}

// ── Velocity / CC lanes ──────────────────────────────────────────────────────

void PianoRollWidget::setVelocityLaneVisible (bool show)
{
    velocityLaneVisible = show;
    resized();
    repaint();
}

void PianoRollWidget::setCCLaneVisible (bool show)
{
    ccLaneVisible = show;
    resized();
    repaint();
}

// ── Animation ────────────────────────────────────────────────────────────────

void PianoRollWidget::animationTick (double)
{
    if (! clipState.isValid())
        return;

    // Update playhead position from transport
    auto posSamples = transportController.getPositionInSamples();
    double sr = project.getSampleRate();
    double tempo = project.getTempo();

    if (sr > 0 && tempo > 0)
    {
        // Convert clip-relative sample position to beat
        int64_t clipStart = clipState.getProperty (IDs::startPosition).getIntOr (0);
        double relativeSamples = static_cast<double> (posSamples - clipStart);
        double relativeSeconds = relativeSamples / sr;
        double relativeBeat = relativeSeconds * tempo / 60.0;

        playheadBeat = relativeBeat;
    }

    // Sync keyboard scroll with scrollView
    keyboard.setScrollOffset (scrollView.getScrollOffsetY());
    ruler.setScrollOffset (scrollView.getScrollOffsetX());

    repaint();
}

void PianoRollWidget::ensureCursorVisible()
{
    float cursorX = noteGrid.beatsToX (static_cast<double> (prBeatCol) / gridDivision);
    float cursorY = noteGrid.noteToY (prNoteRow);
    float cursorW = pixelsPerBeat / static_cast<float> (gridDivision);

    scrollView.scrollToMakeVisible (gfx::Rect (cursorX, cursorY, cursorW, rowHeight));
}

// ── PropertyTree::Listener ───────────────────────────────────────────────────

void PianoRollWidget::propertyChanged (PropertyTree&, PropertyId)
{
    rebuildNotes();
}

void PianoRollWidget::childAdded (PropertyTree&, PropertyTree&)
{
    rebuildNotes();
}

void PianoRollWidget::childRemoved (PropertyTree&, PropertyTree&, int)
{
    rebuildNotes();
}

} // namespace ui
} // namespace dc
