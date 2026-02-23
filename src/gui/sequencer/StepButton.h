#pragma once
#include <JuceHeader.h>

namespace dc
{

class StepButton : public juce::Component
{
public:
    StepButton();

    void paint (juce::Graphics& g) override;

    void setActive (bool a)    { active = a; repaint(); }
    void setVelocity (int v)   { velocity = v; repaint(); }
    void setSelected (bool s)  { selected = s; repaint(); }
    void setPlayhead (bool p)  { playhead = p; repaint(); }

    bool isActive() const     { return active; }
    int  getVelocity() const  { return velocity; }

    std::function<void()> onClick;

    void mouseDown (const juce::MouseEvent& e) override;

    static constexpr int preferredSize = 32;

private:
    bool active   = false;
    int  velocity = 100;
    bool selected = false;
    bool playhead = false;

    juce::Colour getVelocityColour() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepButton)
};

} // namespace dc
