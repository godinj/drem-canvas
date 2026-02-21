#include "PianoRollEditor.h"
#include <cmath>

namespace dc
{

PianoRollEditor::PianoRollEditor()
{
    addAndMakeVisible (keyboard);
    keyboard.setNoteHeight (noteHeight);
}

void PianoRollEditor::setMidiSequence (const juce::MidiMessageSequence& sequence,
                                        double lengthInBeats)
{
    midiSequence = sequence;
    totalBeats = lengthInBeats;
    rebuildNoteComponents();
    resized();
    repaint();
}

juce::MidiMessageSequence PianoRollEditor::getMidiSequence() const
{
    juce::MidiMessageSequence result;

    for (auto* noteComp : noteComponents)
    {
        int noteNum = noteComp->getNoteNumber();
        double startBeat = noteComp->getStartBeat();
        double lengthBeats = noteComp->getLengthInBeats();
        int vel = noteComp->getVelocity();

        auto noteOn = juce::MidiMessage::noteOn (1, noteNum, static_cast<juce::uint8> (vel));
        noteOn.setTimeStamp (startBeat);
        result.addEvent (noteOn);

        auto noteOff = juce::MidiMessage::noteOff (1, noteNum);
        noteOff.setTimeStamp (startBeat + lengthBeats);
        result.addEvent (noteOff);
    }

    result.sort();
    result.updateMatchedPairs();

    return result;
}

void PianoRollEditor::rebuildNoteComponents()
{
    noteComponents.clear();

    // Create a working copy so updateMatchedPairs doesn't modify the original
    juce::MidiMessageSequence seq (midiSequence);
    seq.updateMatchedPairs();

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto* event = seq.getEventPointer (i);
        const auto& msg = event->message;

        if (msg.isNoteOn())
        {
            int noteNum = msg.getNoteNumber();
            double startBeat = msg.getTimeStamp();
            int vel = msg.getVelocity();

            double lengthBeats = 1.0; // Default length

            if (event->noteOffObject != nullptr)
            {
                lengthBeats = event->noteOffObject->message.getTimeStamp() - startBeat;
                if (lengthBeats <= 0.0)
                    lengthBeats = 1.0;
            }

            auto* noteComp = new NoteComponent (noteNum, startBeat, lengthBeats, vel);
            noteComp->onMoved = [this]() { resized(); repaint(); };

            addAndMakeVisible (noteComp);
            noteComponents.add (noteComp);
        }
    }
}

void PianoRollEditor::paint (juce::Graphics& g)
{
    auto gridArea = getLocalBounds().withTrimmedLeft (keyboardWidth);

    // Draw note row backgrounds
    for (int note = 0; note < totalNotes; ++note)
    {
        int y = static_cast<int> (noteToY (127 - note));

        // Alternate between white and black key backgrounds
        int noteInOctave = (127 - note) % 12;
        bool blackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6
                         || noteInOctave == 8 || noteInOctave == 10);

        if (blackKey)
            g.setColour (juce::Colour (0xFF2A2A2A)); // Darker for black keys
        else
            g.setColour (juce::Colour (0xFF333333)); // Lighter for white keys

        g.fillRect (gridArea.getX(), y, gridArea.getWidth(), noteHeight);

        // Draw horizontal grid line
        g.setColour (juce::Colour (0xFF222222));
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()),
                              static_cast<float> (gridArea.getRight()));
    }

    // Draw vertical grid lines at beat divisions
    g.setColour (juce::Colour (0xFF444444));

    double beatStep = 1.0 / static_cast<double> (gridDivision);
    for (double beat = 0.0; beat <= totalBeats; beat += beatStep)
    {
        float x = beatsToX (beat);

        // Stronger line on whole beats
        bool isWholeBeat = (std::fmod (beat, 1.0) < 0.001);
        bool isBarLine = (std::fmod (beat, 4.0) < 0.001); // Assuming 4/4

        if (isBarLine)
            g.setColour (juce::Colour (0xFF666666));
        else if (isWholeBeat)
            g.setColour (juce::Colour (0xFF4A4A4A));
        else
            g.setColour (juce::Colour (0xFF3A3A3A));

        g.drawVerticalLine (static_cast<int> (x), 0.0f, static_cast<float> (getHeight()));
    }
}

void PianoRollEditor::resized()
{
    // Position keyboard on the left
    keyboard.setBounds (0, 0, keyboardWidth, totalNotes * noteHeight);
    keyboard.setNoteHeight (noteHeight);

    // Position each note component in the grid area
    for (auto* noteComp : noteComponents)
    {
        float x = beatsToX (noteComp->getStartBeat());
        float y = noteToY (noteComp->getNoteNumber());
        float w = static_cast<float> (noteComp->getLengthInBeats() * pixelsPerBeat);

        noteComp->setBounds (static_cast<int> (x),
                             static_cast<int> (y),
                             juce::jmax (4, static_cast<int> (w)),
                             noteHeight);
    }
}

void PianoRollEditor::mouseDown (const juce::MouseEvent& e)
{
    // Only handle clicks in the grid area (right of keyboard)
    if (e.x < keyboardWidth)
        return;

    if (currentTool == Tool::Draw)
    {
        double beat = xToBeats (static_cast<float> (e.x));
        int noteNum = yToNote (static_cast<float> (e.y));

        // Snap to grid
        if (snapEnabled && gridDivision > 0)
        {
            double snapSize = 1.0 / static_cast<double> (gridDivision);
            beat = std::floor (beat / snapSize) * snapSize;
        }

        noteNum = juce::jlimit (0, 127, noteNum);

        // Default note length: one grid division
        double noteLength = 1.0 / static_cast<double> (gridDivision);
        if (noteLength <= 0.0)
            noteLength = 1.0;

        auto* noteComp = new NoteComponent (noteNum, beat, noteLength, 100);
        noteComp->onMoved = [this]() { resized(); repaint(); };

        addAndMakeVisible (noteComp);
        noteComponents.add (noteComp);

        resized();
        repaint();
    }
    else if (currentTool == Tool::Erase)
    {
        // Find and remove note at click position
        for (int i = noteComponents.size() - 1; i >= 0; --i)
        {
            if (noteComponents[i]->getBounds().contains (e.getPosition()))
            {
                removeChildComponent (noteComponents[i]);
                noteComponents.remove (i);
                repaint();
                break;
            }
        }
    }
}

void PianoRollEditor::mouseDrag (const juce::MouseEvent& /*e*/)
{
    // Selection drag could be implemented here
}

void PianoRollEditor::mouseUp (const juce::MouseEvent& /*e*/)
{
    // End of selection/interaction
}

double PianoRollEditor::xToBeats (float x) const
{
    return static_cast<double> (x - keyboardWidth) / pixelsPerBeat;
}

float PianoRollEditor::beatsToX (double beats) const
{
    return static_cast<float> (keyboardWidth) + static_cast<float> (beats * pixelsPerBeat);
}

int PianoRollEditor::yToNote (float y) const
{
    // Top of component = note 127, bottom = note 0
    int row = static_cast<int> (y) / noteHeight;
    return 127 - row;
}

float PianoRollEditor::noteToY (int note) const
{
    return static_cast<float> ((127 - note) * noteHeight);
}

} // namespace dc
