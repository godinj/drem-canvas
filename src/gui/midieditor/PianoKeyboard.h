#pragma once
#include <JuceHeader.h>

namespace dc
{

class PianoKeyboard : public juce::Component
{
public:
    PianoKeyboard();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void setNoteHeight (int h) { noteHeight = h; repaint(); }

    std::function<void (int noteNumber)> onNoteOn;
    std::function<void (int noteNumber)> onNoteOff;

private:
    bool isBlackKey (int noteNumber) const;
    int noteHeight = 12;

    int pressedNote = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboard)
};

} // namespace dc
