#include "StepButton.h"
#include "gui/common/ColourBridge.h"

using dc::bridge::toJuce;

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
        g.setColour (toJuce (getVelocityColour()));
        g.fillRoundedRectangle (bounds, 3.0f);
    }
    else
    {
        g.setColour (toJuce (0xff2a2a3au));
        g.fillRoundedRectangle (bounds, 3.0f);
    }

    // Playback highlight: white overlay
    if (playhead)
    {
        g.setColour (toJuce (dc::Colours::white.withAlpha (0.35f)));
        g.fillRoundedRectangle (bounds, 3.0f);
    }

    // Selection border (green)
    if (selected)
    {
        g.setColour (toJuce (0xff50c878u));
        g.drawRoundedRectangle (bounds, 3.0f, 2.0f);
    }
}

void StepButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

dc::Colour StepButton::getVelocityColour() const
{
    // gray (low) -> orange (mid) -> red (high)
    float t = juce::jlimit (0.0f, 1.0f, static_cast<float> (velocity) / 127.0f);

    if (t < 0.5f)
    {
        // gray to orange
        float u = t * 2.0f;
        return dc::Colour (0xff666666u).interpolatedWith (dc::Colour (0xffff8c00u), u);
    }
    else
    {
        // orange to red
        float u = (t - 0.5f) * 2.0f;
        return dc::Colour (0xffff8c00u).interpolatedWith (dc::Colour (0xffff2020u), u);
    }
}

} // namespace dc
