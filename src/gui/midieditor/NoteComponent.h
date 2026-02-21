#pragma once
#include <JuceHeader.h>

namespace dc
{

class NoteComponent : public juce::Component
{
public:
    NoteComponent (int noteNumber, double startBeat, double lengthInBeats, int velocity = 100);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    int getNoteNumber() const  { return noteNumber; }
    double getStartBeat() const { return startBeat; }
    double getLengthInBeats() const { return lengthInBeats; }
    int getVelocity() const { return velocity; }

    void setSelected (bool s) { selected = s; repaint(); }
    bool isSelected() const { return selected; }

    // Callback for when note is moved/resized
    std::function<void()> onMoved;

private:
    int noteNumber;
    double startBeat;
    double lengthInBeats;
    int velocity;
    bool selected = false;

    juce::Point<int> dragStart;
    bool resizing = false;
    double originalStartBeat = 0.0;
    double originalLength = 0.0;

    static constexpr int resizeHandleWidth = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteComponent)
};

} // namespace dc
