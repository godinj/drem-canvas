#pragma once
#include <JuceHeader.h>
#include "NoteComponent.h"
#include "PianoKeyboard.h"

namespace dc
{

class PianoRollEditor : public juce::Component
{
public:
    PianoRollEditor();

    void setMidiSequence (const juce::MidiMessageSequence& sequence, double lengthInBeats);
    juce::MidiMessageSequence getMidiSequence() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void setPixelsPerBeat (double ppb) { pixelsPerBeat = ppb; resized(); }
    void setNoteHeight (int h) { noteHeight = h; resized(); }

    // Snap settings
    void setSnapToGrid (bool snap) { snapEnabled = snap; }
    void setGridDivision (int division) { gridDivision = division; repaint(); }

private:
    void rebuildNoteComponents();
    double xToBeats (float x) const;
    float beatsToX (double beats) const;
    int yToNote (float y) const;
    float noteToY (int note) const;

    juce::MidiMessageSequence midiSequence;
    juce::OwnedArray<NoteComponent> noteComponents;

    PianoKeyboard keyboard;

    double pixelsPerBeat = 40.0;
    int noteHeight = 12;
    double totalBeats = 16.0;
    bool snapEnabled = true;
    int gridDivision = 4; // quarter note grid

    static constexpr int keyboardWidth = 60;
    static constexpr int totalNotes = 128;

    // Editing state
    enum class Tool { Select, Draw, Erase };
    Tool currentTool = Tool::Draw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollEditor)
};

} // namespace dc
