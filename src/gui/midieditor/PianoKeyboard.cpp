#include "PianoKeyboard.h"

namespace dc
{

PianoKeyboard::PianoKeyboard()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PianoKeyboard::paint (juce::Graphics& g)
{
    // Draw 128 note rows from top (note 127) to bottom (note 0)
    for (int note = 127; note >= 0; --note)
    {
        int y = (127 - note) * noteHeight;

        // Background colour
        if (isBlackKey (note))
            g.setColour (juce::Colour (0xFF3A3A3A)); // Dark grey for black keys
        else
            g.setColour (juce::Colour (0xFF5A5A5A)); // Lighter grey for white keys

        // Highlight pressed note
        if (note == pressedNote)
            g.setColour (juce::Colour (0xFF6699CC));

        g.fillRect (0, y, getWidth(), noteHeight);

        // Draw separator line
        g.setColour (juce::Colour (0xFF2A2A2A));
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));

        // Draw note name for C notes
        if (note % 12 == 0)
        {
            int octave = (note / 12) - 1; // MIDI note 0 = C-1
            juce::String noteName = "C" + juce::String (octave);

            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (static_cast<float> (juce::jmin (noteHeight - 1, 11))));
            g.drawText (noteName, 2, y, getWidth() - 4, noteHeight,
                        juce::Justification::centredLeft, true);
        }
    }
}

bool PianoKeyboard::isBlackKey (int noteNumber) const
{
    int note = noteNumber % 12;
    // C#=1, D#=3, F#=6, G#=8, A#=10
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

void PianoKeyboard::mouseDown (const juce::MouseEvent& e)
{
    int note = 127 - (e.y / noteHeight);
    note = juce::jlimit (0, 127, note);

    pressedNote = note;
    repaint();

    if (onNoteOn)
        onNoteOn (note);
}

void PianoKeyboard::mouseUp (const juce::MouseEvent& /*e*/)
{
    if (pressedNote >= 0 && onNoteOff)
        onNoteOff (pressedNote);

    pressedNote = -1;
    repaint();
}

} // namespace dc
