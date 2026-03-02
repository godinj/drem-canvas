#pragma once
#include "Project.h"
#include "dc/midi/MidiSequence.h"

namespace dc
{

class MidiClip
{
public:
    explicit MidiClip (const PropertyTree& state);

    bool isValid() const { return state.isValid(); }

    int64_t getStartPosition() const;
    void setStartPosition (int64_t pos, UndoManager* um = nullptr);
    int64_t getLength() const;
    void setLength (int64_t len, UndoManager* um = nullptr);

    // MIDI data stored as base64-encoded binary
    MidiSequence getMidiSequence() const;
    void setMidiSequence (const MidiSequence& seq, UndoManager* um = nullptr);

    // Data bridge: NOTE children ←→ base64 midiData
    // expandNotesToChildren() decodes base64 into NOTE PropertyTree children for editing.
    // collapseChildrenToMidiData() encodes NOTE children back into base64 for storage/playback.
    void expandNotesToChildren();
    void collapseChildrenToMidiData (UndoManager* um = nullptr);

    // Note editing (operates on NOTE children, then collapses)
    PropertyTree addNote (int noteNumber, double startBeat, double lengthBeats,
                          int velocity, UndoManager* um = nullptr);
    void removeNote (int childIndex, UndoManager* um = nullptr);

    PropertyTree& getState() { return state; }

private:
    PropertyTree state;
};

} // namespace dc
