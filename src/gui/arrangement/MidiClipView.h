#pragma once
#include <JuceHeader.h>

namespace dc
{

class MidiClipView : public juce::Component
{
public:
    MidiClipView();

    void setMidiSequence (const juce::MidiMessageSequence& sequence);
    void setClipColour (juce::Colour c) { clipColour = c; repaint(); }

    void paint (juce::Graphics& g) override;

private:
    juce::MidiMessageSequence sequence;
    juce::Colour clipColour { juce::Colours::mediumpurple };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClipView)
};

} // namespace dc
