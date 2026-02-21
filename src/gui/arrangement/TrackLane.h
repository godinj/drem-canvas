#pragma once
#include <JuceHeader.h>
#include "WaveformView.h"
#include "model/Project.h"

namespace dc
{

class TrackLane : public juce::Component,
                  private juce::ValueTree::Listener
{
public:
    explicit TrackLane (const juce::ValueTree& trackState);
    ~TrackLane() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Pixels per second for zoom control
    void setPixelsPerSecond (double pps);
    double getPixelsPerSecond() const { return pixelsPerSecond; }

    void setSampleRate (double sr) { sampleRate = sr; }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void rebuildClipViews();

    juce::ValueTree trackState;
    juce::OwnedArray<WaveformView> clipViews;

    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;

    static constexpr int headerWidth = 150;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLane)
};

} // namespace dc
