#include "StepButton.h"

namespace dc
{

StepButton::StepButton()
{
}

void StepButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (1).toFloat();

    if (active)
    {
        g.setColour (getVelocityColour());
        g.fillRoundedRectangle (bounds, 3.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff2a2a3a));
        g.fillRoundedRectangle (bounds, 3.0f);
    }

    // Playback highlight: white overlay
    if (playhead)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds, 3.0f);
    }

    // Selection border (cyan)
    if (selected)
    {
        g.setColour (juce::Colour (0xff00e5ff));
        g.drawRoundedRectangle (bounds, 3.0f, 2.0f);
    }
}

void StepButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

juce::Colour StepButton::getVelocityColour() const
{
    // gray (low) -> orange (mid) -> red (high)
    float t = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / 127.0f);

    if (t < 0.5f)
    {
        // gray to orange
        float u = t * 2.0f;
        return juce::Colour (0xff666666).interpolatedWith (juce::Colour (0xffff8c00), u);
    }
    else
    {
        // orange to red
        float u = (t - 0.5f) * 2.0f;
        return juce::Colour (0xffff8c00).interpolatedWith (juce::Colour (0xffff2020), u);
    }
}

} // namespace dc
