#pragma once

#include <set>
#include <JuceHeader.h>

namespace dc
{

class VirtualKeyboardState
{
public:
    int baseOctave = 4;
    int velocity = 100;
    int midiChannel = 1;

    std::set<int> heldNotes;

    // Cubase-style two-row QWERTY → MIDI note mapping.
    // Returns -1 if key is not a piano key.
    int keyToNote (juce_wchar key) const
    {
        int semitone = -1;

        switch (key)
        {
            // Bottom row: white keys
            case 'a': case 'A': semitone =  0; break; // C
            case 's': case 'S': semitone =  2; break; // D
            case 'd': case 'D': semitone =  4; break; // E
            case 'f': case 'F': semitone =  5; break; // F
            case 'g': case 'G': semitone =  7; break; // G
            case 'h': case 'H': semitone =  9; break; // A
            case 'j': case 'J': semitone = 11; break; // B

            // Top row: black keys
            case 'w': case 'W': semitone =  1; break; // C#
            case 'e': case 'E': semitone =  3; break; // D#
            case 't': case 'T': semitone =  6; break; // F#
            case 'y': case 'Y': semitone =  8; break; // G#
            case 'u': case 'U': semitone = 10; break; // A#

            // Upper octave: white keys
            case 'k': case 'K': semitone = 12; break; // C+1
            case 'l': case 'L': semitone = 14; break; // D+1
            case ';':           semitone = 16; break; // E+1

            // Upper octave: black keys
            case 'o': case 'O': semitone = 13; break; // C#+1
            case 'p': case 'P': semitone = 15; break; // D#+1

            default: return -1;
        }

        int note = baseOctave * 12 + semitone;
        return (note >= 0 && note <= 127) ? note : -1;
    }

    void octaveDown()
    {
        if (baseOctave > 0) --baseOctave;
    }

    void octaveUp()
    {
        if (baseOctave < 9) ++baseOctave;
    }

    void velocityDown()
    {
        velocity = std::max (10, velocity - 10);
    }

    void velocityUp()
    {
        velocity = std::min (127, velocity + 10);
    }

    // Listener for UI updates
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void keyboardStateChanged() = 0;
    };

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    void notifyListeners()
    {
        listeners.call (&Listener::keyboardStateChanged);
    }

private:
    juce::ListenerList<Listener> listeners;
};

} // namespace dc
