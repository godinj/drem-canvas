#include "MeterComponent.h"

namespace dc
{

MeterComponent::MeterComponent()
{
    startTimerHz (30);
}

void MeterComponent::timerCallback()
{
    // Read levels from callbacks, defaulting to 0 if not set
    const float levelLeft  = getLeftLevel  ? getLeftLevel()  : 0.0f;
    const float levelRight = getRightLevel ? getRightLevel() : 0.0f;

    // Smooth display values with decay
    displayLeft  = std::max (levelLeft,  displayLeft  * 0.85f);
    displayRight = std::max (levelRight, displayRight * 0.85f);

    // Update hold for left channel
    if (levelLeft > holdLeft)
    {
        holdLeft = levelLeft;
        holdCountLeft = 0;
    }
    else
    {
        ++holdCountLeft;
        if (holdCountLeft > holdFrames)
            holdLeft = std::max (0.0f, holdLeft * 0.95f);
    }

    // Update hold for right channel
    if (levelRight > holdRight)
    {
        holdRight = levelRight;
        holdCountRight = 0;
    }
    else
    {
        ++holdCountRight;
        if (holdCountRight > holdFrames)
            holdRight = std::max (0.0f, holdRight * 0.95f);
    }

    repaint();
}

void MeterComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float meterGap = 2.0f;
    const float meterWidth = (bounds.getWidth() - meterGap) * 0.5f;
    const float meterHeight = bounds.getHeight();

    // dB markings area on the right
    const float dbLabelWidth = 24.0f;
    const float usableWidth = bounds.getWidth() - dbLabelWidth;
    const float barWidth = (usableWidth - meterGap) * 0.5f;

    auto leftBarBounds  = juce::Rectangle<float> (bounds.getX(), bounds.getY(), barWidth, meterHeight);
    auto rightBarBounds = juce::Rectangle<float> (bounds.getX() + barWidth + meterGap, bounds.getY(), barWidth, meterHeight);

    // Helper lambda to draw a single meter bar
    auto drawBar = [&] (juce::Rectangle<float> area, float displayLevel, float holdLevel)
    {
        // Background
        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillRect (area);

        // Clamp display level to 0..1 range for drawing
        const float clampedLevel = juce::jlimit (0.0f, 1.0f, displayLevel);
        const float filledHeight = clampedLevel * area.getHeight();

        if (filledHeight > 0.0f)
        {
            // Create gradient: green (bottom) -> yellow (middle) -> red (top)
            juce::ColourGradient gradient (juce::Colours::green,
                                           area.getX(), area.getBottom(),
                                           juce::Colours::red,
                                           area.getX(), area.getY(),
                                           false);
            gradient.addColour (0.5, juce::Colours::yellow);

            g.setGradientFill (gradient);
            g.fillRect (area.getX(),
                        area.getBottom() - filledHeight,
                        area.getWidth(),
                        filledHeight);
        }

        // Hold indicator line
        const float clampedHold = juce::jlimit (0.0f, 1.0f, holdLevel);
        if (clampedHold > 0.01f)
        {
            const float holdY = area.getBottom() - (clampedHold * area.getHeight());
            g.setColour (juce::Colours::white);
            g.drawHorizontalLine (static_cast<int> (holdY), area.getX(), area.getRight());
        }
    };

    drawBar (leftBarBounds, displayLeft, holdLeft);
    drawBar (rightBarBounds, displayRight, holdRight);

    // Draw dB markings
    g.setColour (juce::Colour (0xffaaaaaa));
    g.setFont (juce::Font (9.0f));

    const float labelX = bounds.getRight() - dbLabelWidth;

    struct DbMark { float db; const char* label; };
    const DbMark marks[] = {
        {   0.0f, " 0"  },
        {  -6.0f, "-6"  },
        { -12.0f, "-12" },
        { -24.0f, "-24" },
        { -48.0f, "-48" }
    };

    for (const auto& mark : marks)
    {
        // Convert dB to linear then to Y position
        const float linear = juce::Decibels::decibelsToGain (mark.db);
        const float y = bounds.getBottom() - (linear * meterHeight);

        if (y >= bounds.getY() && y <= bounds.getBottom())
        {
            g.drawText (mark.label,
                        static_cast<int> (labelX),
                        static_cast<int> (y - 6),
                        static_cast<int> (dbLabelWidth),
                        12,
                        juce::Justification::centredLeft);

            // Draw tick line across meters
            g.setColour (juce::Colour (0xff555555));
            g.drawHorizontalLine (static_cast<int> (y),
                                  bounds.getX(),
                                  bounds.getX() + usableWidth);
            g.setColour (juce::Colour (0xffaaaaaa));
        }
    }
}

} // namespace dc
