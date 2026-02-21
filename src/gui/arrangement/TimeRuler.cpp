#include "TimeRuler.h"

namespace dc
{

TimeRuler::TimeRuler()
{
}

void TimeRuler::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour (juce::Colour (0xff252535));
    g.fillRect (bounds);

    // Calculate visible time range (offset by header width)
    double startTime = scrollOffset / pixelsPerSecond;
    double endTime = startTime + static_cast<double> (getWidth() - headerWidth) / pixelsPerSecond;

    // Choose tick interval based on zoom level
    double tickInterval;

    if (pixelsPerSecond >= 200.0)
        tickInterval = 1.0;
    else if (pixelsPerSecond >= 50.0)
        tickInterval = 5.0;
    else if (pixelsPerSecond >= 20.0)
        tickInterval = 10.0;
    else if (pixelsPerSecond >= 5.0)
        tickInterval = 30.0;
    else
        tickInterval = 60.0;

    // Round start time down to nearest tick
    double firstTick = std::floor (startTime / tickInterval) * tickInterval;

    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::Font (11.0f));

    for (double t = firstTick; t <= endTime; t += tickInterval)
    {
        if (t < 0.0)
            continue;

        float x = static_cast<float> ((t - startTime) * pixelsPerSecond) + static_cast<float> (headerWidth);

        // Draw tick mark
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (bounds.getHeight()) * 0.5f,
                            static_cast<float> (bounds.getHeight()) - 2.0f);

        // Format time as MM:SS
        int totalSeconds = juce::roundToInt (t);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;

        juce::String timeLabel = juce::String::formatted ("%02d:%02d", minutes, seconds);

        g.drawText (timeLabel,
                    juce::roundToInt (x) + 3, 0,
                    60, bounds.getHeight() - 4,
                    juce::Justification::centredLeft, false);

        // Draw minor ticks between major ticks
        if (tickInterval >= 5.0)
        {
            for (double minor = t + 1.0; minor < t + tickInterval && minor <= endTime; minor += 1.0)
            {
                float mx = static_cast<float> ((minor - startTime) * pixelsPerSecond) + static_cast<float> (headerWidth);
                g.setColour (juce::Colours::lightgrey.withAlpha (0.3f));
                g.drawVerticalLine (juce::roundToInt (mx),
                                    static_cast<float> (bounds.getHeight()) * 0.75f,
                                    static_cast<float> (bounds.getHeight()) - 2.0f);
                g.setColour (juce::Colours::lightgrey);
            }
        }
    }

    // Bottom border line
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine (bounds.getHeight() - 1, 0.0f, static_cast<float> (bounds.getWidth()));
}

void TimeRuler::seekFromMouseEvent (const juce::MouseEvent& event)
{
    if (event.x < headerWidth)
        return;

    double timeInSeconds = (static_cast<double> (event.x - headerWidth) + scrollOffset) / pixelsPerSecond;

    if (onSeek && timeInSeconds >= 0.0)
        onSeek (timeInSeconds);
}

void TimeRuler::mouseDown (const juce::MouseEvent& event)
{
    seekFromMouseEvent (event);
}

void TimeRuler::mouseDrag (const juce::MouseEvent& event)
{
    seekFromMouseEvent (event);
}

} // namespace dc
