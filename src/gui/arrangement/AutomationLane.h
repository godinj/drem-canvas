#pragma once
#include <JuceHeader.h>

namespace dc
{

struct AutomationPoint
{
    double time;       // in seconds
    float value;       // 0.0 to 1.0
    float curve = 0.0f; // -1.0 to 1.0, 0 = linear
};

class AutomationLane : public juce::Component
{
public:
    AutomationLane();

    void setParameterName (const juce::String& name) { paramName = name; }
    void setPixelsPerSecond (double pps) { pixelsPerSecond = pps; repaint(); }
    void setSampleRate (double sr) { sampleRate = sr; }

    void addPoint (double time, float value);
    void removePoint (int index);
    void clearPoints();

    float getValueAtTime (double time) const;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    float timeToX (double time) const;
    double xToTime (float x) const;
    float valueToY (float value) const;
    float yToValue (float y) const;

    juce::String paramName;
    std::vector<AutomationPoint> points;
    int draggedPointIndex = -1;
    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationLane)
};

} // namespace dc
