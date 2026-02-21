#pragma once
#include <JuceHeader.h>

namespace dc
{

class Cursor : public juce::Component
{
public:
    Cursor();

    void paint (juce::Graphics& g) override;
    void setCursorColour (juce::Colour c) { cursorColour = c; repaint(); }

private:
    juce::Colour cursorColour { juce::Colours::red };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Cursor)
};

} // namespace dc
