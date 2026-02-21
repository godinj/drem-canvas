#pragma once
#include <JuceHeader.h>

namespace dc
{

class TimeRuler : public juce::Component
{
public:
    TimeRuler();

    void setPixelsPerSecond (double pps) { pixelsPerSecond = pps; repaint(); }
    void setScrollOffset (double offset) { scrollOffset = offset; repaint(); }

    void paint (juce::Graphics& g) override;

private:
    double pixelsPerSecond = 100.0;
    double scrollOffset = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeRuler)
};

} // namespace dc
