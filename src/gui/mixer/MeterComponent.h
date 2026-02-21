#pragma once
#include <JuceHeader.h>
#include <functional>

namespace dc
{

class MeterComponent : public juce::Component,
                       private juce::Timer
{
public:
    MeterComponent();

    void paint (juce::Graphics& g) override;

    // Call this to provide peak level source
    std::function<float()> getLeftLevel;
    std::function<float()> getRightLevel;

private:
    void timerCallback() override;

    float displayLeft = 0.0f;
    float displayRight = 0.0f;
    float holdLeft = 0.0f;
    float holdRight = 0.0f;
    int holdCountLeft = 0;
    int holdCountRight = 0;

    static constexpr int holdFrames = 30; // ~1 second at 30fps

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterComponent)
};

} // namespace dc
