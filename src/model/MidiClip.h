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

    juce::ValueTree& getState() { return state; }

private:
    juce::ValueTree state;
};

} // namespace dc
