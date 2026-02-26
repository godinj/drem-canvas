#pragma once
#include <JuceHeader.h>
#include "Project.h"

namespace dc
{

class MidiClip
{
public:
    explicit MidiClip (const juce::ValueTree& state);

    bool isValid() const { return state.isValid(); }

    int64_t getStartPosition() const;
    void setStartPosition (int64_t pos, juce::UndoManager* um = nullptr);
    int64_t getLength() const;
    void setLength (int64_t len, juce::UndoManager* um = nullptr);

    // MIDI data stored as base64-encoded MidiMessageSequence
    juce::MidiMessageSequence getMidiSequence() const;
    void setMidiSequence (const juce::MidiMessageSequence& seq, juce::UndoManager* um = nullptr);

    // Data bridge: NOTE children ←→ base64 midiData
    // expandNotesToChildren() decodes base64 into NOTE ValueTree children for editing.
    // collapseChildrenToMidiData() encodes NOTE children back into base64 for storage/playback.
    void expandNotesToChildren();
    void collapseChildrenToMidiData (juce::UndoManager* um = nullptr);

    // Note editing (operates on NOTE children, then collapses)
    juce::ValueTree addNote (int noteNumber, double startBeat, double lengthBeats,
                             int velocity, juce::UndoManager* um = nullptr);
    void removeNote (int childIndex, juce::UndoManager* um = nullptr);

    juce::ValueTree& getState() { return state; }

private:
    juce::ValueTree state;
};

} // namespace dc
