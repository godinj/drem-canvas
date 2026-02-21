#include "DremLookAndFeel.h"

namespace dc
{

DremLookAndFeel::DremLookAndFeel()
{
    // Dark colour scheme
    auto scheme = LookAndFeel_V4::ColourScheme {
        0xff1e1e2e,  // windowBackground
        0xff2a2a3a,  // widgetBackground
        0xff252535,  // menuBackground
        0xff3a3a4a,  // outline
        0xffe0e0e0,  // defaultText
        0xff4a9eff,  // defaultFill (accent blue)
        0xffffffff,  // highlightedText
        0xff5ab0ff,  // highlightedFill
        0xffe0e0e0   // menuText
    };

    setColourScheme (scheme);
}

void DremLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour& /*backgroundColour*/,
                                            bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float cornerRadius = 4.0f;

    juce::Colour bg;

    if (button.getToggleState())
    {
        // Toggled on: accent colour
        bg = juce::Colour (0xff4a9eff);
    }
    else if (isButtonDown)
    {
        bg = juce::Colour (0xff5a5a6a);
    }
    else if (isMouseOverButton)
    {
        bg = juce::Colour (0xff4a4a5a);
    }
    else
    {
        bg = juce::Colour (0xff3a3a4a);
    }

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, cornerRadius);
}

void DremLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPos,
                                        float /*minSliderPos*/, float /*maxSliderPos*/,
                                        juce::Slider::SliderStyle style,
                                        juce::Slider& /*slider*/)
{
    const bool isVertical = (style == juce::Slider::LinearVertical);

    if (isVertical)
    {
        // Draw track groove (thin vertical line in the centre)
        const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const float trackWidth = 4.0f;

        g.setColour (juce::Colour (0xff555565));
        g.fillRoundedRectangle (centreX - trackWidth * 0.5f,
                                static_cast<float> (y),
                                trackWidth,
                                static_cast<float> (height),
                                2.0f);

        // Draw thumb as rounded rectangle at sliderPos
        const float thumbWidth = 20.0f;
        const float thumbHeight = 10.0f;
        const float thumbY = sliderPos - thumbHeight * 0.5f;

        g.setColour (juce::Colour (0xff4a9eff));
        g.fillRoundedRectangle (centreX - thumbWidth * 0.5f,
                                thumbY,
                                thumbWidth,
                                thumbHeight,
                                3.0f);
    }
    else
    {
        // Horizontal slider
        const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
        const float trackHeight = 4.0f;

        g.setColour (juce::Colour (0xff555565));
        g.fillRoundedRectangle (static_cast<float> (x),
                                centreY - trackHeight * 0.5f,
                                static_cast<float> (width),
                                trackHeight,
                                2.0f);

        // Thumb
        const float thumbWidth = 10.0f;
        const float thumbHeight = 20.0f;
        const float thumbX = sliderPos - thumbWidth * 0.5f;

        g.setColour (juce::Colour (0xff4a9eff));
        g.fillRoundedRectangle (thumbX,
                                centreY - thumbHeight * 0.5f,
                                thumbWidth,
                                thumbHeight,
                                3.0f);
    }
}

void DremLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& /*slider*/)
{
    const float radius = static_cast<float> (juce::jmin (width, height)) * 0.5f - 4.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Background circle
    g.setColour (juce::Colour (0xff3a3a4a));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    // Filled arc from start to current position
    const float arcThickness = 3.0f;
    juce::Path arcPath;
    arcPath.addCentredArc (centreX, centreY,
                           radius - arcThickness * 0.5f,
                           radius - arcThickness * 0.5f,
                           0.0f,
                           rotaryStartAngle,
                           angle,
                           true);

    g.setColour (juce::Colour (0xff4a9eff));
    g.strokePath (arcPath, juce::PathStrokeType (arcThickness,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // Dot indicator at current position
    const float dotRadius = 3.0f;
    const float dotDistance = radius - arcThickness * 0.5f;
    const float dotX = centreX + dotDistance * std::cos (angle - juce::MathConstants<float>::halfPi);
    const float dotY = centreY + dotDistance * std::sin (angle - juce::MathConstants<float>::halfPi);

    g.setColour (juce::Colours::white);
    g.fillEllipse (dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

} // namespace dc
