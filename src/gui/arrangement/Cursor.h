#pragma once
#include <JuceHeader.h>
#include "dc/foundation/types.h"

namespace dc
{

class Cursor : public juce::Component
{
public:
    Cursor();

    void paint (juce::Graphics& g) override;
    void setCursorColour (dc::Colour c) { cursorColour = c; repaint(); }

private:
    dc::Colour cursorColour { dc::Colours::red };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Cursor)
};

} // namespace dc
