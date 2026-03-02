#pragma once
#include <JuceHeader.h>
#include "dc/foundation/types.h"
#include "dc/midi/MidiSequence.h"

namespace dc
{

class MidiClipView : public juce::Component
{
public:
    MidiClipView();

    void setMidiSequence (const dc::MidiSequence& sequence);
    void setClipColour (dc::Colour c) { clipColour = c; repaint(); }

    void paint (juce::Graphics& g) override;

private:
    dc::MidiSequence sequence;
    dc::Colour clipColour { dc::Colours::mediumpurple };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClipView)
};

} // namespace dc
