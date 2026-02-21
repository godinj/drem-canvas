#include "AutomationLane.h"

namespace dc
{

AutomationLane::AutomationLane()
{
    setOpaque (false);
}

void AutomationLane::addPoint (double time, float value)
{
    AutomationPoint pt;
    pt.time  = time;
    pt.value = juce::jlimit (0.0f, 1.0f, value);

    // Insert sorted by time
    auto it = std::lower_bound (points.begin(), points.end(), pt,
        [] (const AutomationPoint& a, const AutomationPoint& b)
        {
            return a.time < b.time;
        });

    points.insert (it, pt);
    repaint();
}

void AutomationLane::removePoint (int index)
{
    if (index >= 0 && index < static_cast<int> (points.size()))
    {
        points.erase (points.begin() + index);
        repaint();
    }
}

void AutomationLane::clearPoints()
{
    points.clear();
    repaint();
}

float AutomationLane::getValueAtTime (double time) const
{
    if (points.empty())
        return 0.5f;

    // Before the first point
    if (time <= points.front().time)
        return points.front().value;

    // After the last point
    if (time >= points.back().time)
        return points.back().value;

    // Find the surrounding points
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto& p0 = points[i];
        const auto& p1 = points[i + 1];

        if (time >= p0.time && time <= p1.time)
        {
            double segmentLength = p1.time - p0.time;
            if (segmentLength <= 0.0)
                return p0.value;

            // Normalised position within the segment [0, 1]
            float t = static_cast<float> ((time - p0.time) / segmentLength);

            // Apply curve shaping.  curve == 0 gives linear interpolation.
            // Positive curve bows upward, negative bows downward.
            if (std::abs (p0.curve) > 0.001f)
            {
                // Attempt exponential curve: t' = t^(2^(-curve))
                float exponent = std::pow (2.0f, -p0.curve);
                t = std::pow (t, exponent);
            }

            return p0.value + t * (p1.value - p0.value);
        }
    }

    return 0.5f;
}

//==============================================================================
// Coordinate conversions
//==============================================================================

float AutomationLane::timeToX (double time) const
{
    return static_cast<float> (time * pixelsPerSecond);
}

double AutomationLane::xToTime (float x) const
{
    return static_cast<double> (x) / pixelsPerSecond;
}

float AutomationLane::valueToY (float value) const
{
    // Value 1.0 maps to the top (y = 0), value 0.0 maps to bottom (y = height)
    return static_cast<float> (getHeight()) * (1.0f - value);
}

float AutomationLane::yToValue (float y) const
{
    if (getHeight() <= 0)
        return 0.5f;

    return juce::jlimit (0.0f, 1.0f,
        1.0f - (y / static_cast<float> (getHeight())));
}

//==============================================================================
// Painting
//==============================================================================

void AutomationLane::paint (juce::Graphics& g)
{
    // Translucent dark background
    g.setColour (juce::Colour (0x30000000));
    g.fillRect (getLocalBounds());

    // Draw parameter name in top-left
    if (paramName.isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (12.0f));
        g.drawText (paramName, 4, 2, 200, 16, juce::Justification::centredLeft, true);
    }

    if (points.empty())
        return;

    // Build a path through all points
    juce::Path automationPath;
    automationPath.startNewSubPath (timeToX (points[0].time), valueToY (points[0].value));

    for (size_t i = 1; i < points.size(); ++i)
    {
        automationPath.lineTo (timeToX (points[i].time), valueToY (points[i].value));
    }

    // Draw the automation line
    g.setColour (juce::Colours::cyan.withAlpha (0.9f));
    g.strokePath (automationPath, juce::PathStrokeType (2.0f));

    // Draw points as circles
    constexpr float pointRadius = 6.0f;

    for (int i = 0; i < static_cast<int> (points.size()); ++i)
    {
        float px = timeToX (points[static_cast<size_t> (i)].time);
        float py = valueToY (points[static_cast<size_t> (i)].value);

        if (i == draggedPointIndex)
        {
            // Highlighted / selected point
            g.setColour (juce::Colours::yellow);
            g.fillEllipse (px - pointRadius, py - pointRadius,
                           pointRadius * 2.0f, pointRadius * 2.0f);
        }
        else
        {
            g.setColour (juce::Colours::cyan);
            g.fillEllipse (px - pointRadius, py - pointRadius,
                           pointRadius * 2.0f, pointRadius * 2.0f);
        }

        // Outline
        g.setColour (juce::Colours::white);
        g.drawEllipse (px - pointRadius, py - pointRadius,
                       pointRadius * 2.0f, pointRadius * 2.0f, 1.0f);
    }
}

//==============================================================================
// Mouse interaction
//==============================================================================

void AutomationLane::mouseDown (const juce::MouseEvent& e)
{
    draggedPointIndex = -1;

    constexpr float hitRadius = 10.0f;
    float mx = static_cast<float> (e.x);
    float my = static_cast<float> (e.y);

    // Find the nearest point within hit radius
    float bestDistSq = hitRadius * hitRadius;

    for (int i = 0; i < static_cast<int> (points.size()); ++i)
    {
        float px = timeToX (points[static_cast<size_t> (i)].time);
        float py = valueToY (points[static_cast<size_t> (i)].value);

        float dx = mx - px;
        float dy = my - py;
        float distSq = dx * dx + dy * dy;

        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            draggedPointIndex = i;
        }
    }

    // Right-click to delete a point
    if (e.mods.isPopupMenu() && draggedPointIndex >= 0)
    {
        removePoint (draggedPointIndex);
        draggedPointIndex = -1;
        return;
    }

    repaint();
}

void AutomationLane::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedPointIndex < 0 || draggedPointIndex >= static_cast<int> (points.size()))
        return;

    double newTime = xToTime (static_cast<float> (e.x));
    float newValue = yToValue (static_cast<float> (e.y));

    // Clamp time to be non-negative
    newTime = std::max (0.0, newTime);

    auto& pt = points[static_cast<size_t> (draggedPointIndex)];
    pt.time  = newTime;
    pt.value = juce::jlimit (0.0f, 1.0f, newValue);

    // Keep points sorted – swap with neighbours if needed
    while (draggedPointIndex > 0
           && points[static_cast<size_t> (draggedPointIndex)].time
              < points[static_cast<size_t> (draggedPointIndex - 1)].time)
    {
        std::swap (points[static_cast<size_t> (draggedPointIndex)],
                   points[static_cast<size_t> (draggedPointIndex - 1)]);
        --draggedPointIndex;
    }

    while (draggedPointIndex < static_cast<int> (points.size()) - 1
           && points[static_cast<size_t> (draggedPointIndex)].time
              > points[static_cast<size_t> (draggedPointIndex + 1)].time)
    {
        std::swap (points[static_cast<size_t> (draggedPointIndex)],
                   points[static_cast<size_t> (draggedPointIndex + 1)]);
        ++draggedPointIndex;
    }

    repaint();
}

void AutomationLane::mouseUp (const juce::MouseEvent&)
{
    draggedPointIndex = -1;
    repaint();
}

void AutomationLane::mouseDoubleClick (const juce::MouseEvent& e)
{
    double time  = xToTime (static_cast<float> (e.x));
    float  value = yToValue (static_cast<float> (e.y));

    addPoint (time, value);
}

} // namespace dc
