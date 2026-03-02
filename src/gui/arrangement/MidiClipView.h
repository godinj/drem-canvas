#pragma once
#include <JuceHeader.h>
#include "dc/foundation/types.h"

namespace dc
{

class MidiClipView : public juce::Component
{
public:
    MidiClipView();

    void setMidiSequence (const juce::MidiMessageSequence& sequence);
    void setClipColour (dc::Colour c) { clipColour = c; repaint(); }

    void paint (juce::Graphics& g) override;

private:
    juce::MidiMessageSequence sequence;
    dc::Colour clipColour { dc::Colours::mediumpurple };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClipView)
};

} // namespace dc
