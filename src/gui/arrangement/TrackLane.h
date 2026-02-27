#pragma once
#include <JuceHeader.h>
#include "WaveformView.h"
#include "model/Project.h"
#include "vim/VimContext.h"

namespace dc
{

class TrackLane : public juce::Component,
                  private juce::ValueTree::Listener
{
public:
    explicit TrackLane (const juce::ValueTree& trackState);
    ~TrackLane() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

    // Pixels per second for zoom control
    void setPixelsPerSecond (double pps);
    double getPixelsPerSecond() const { return pixelsPerSecond; }

    void setSampleRate (double sr) { sampleRate = sr; }

    // Vim cursor selection
    void setSelected (bool shouldBeSelected);
    void setSelectedClipIndex (int index);
    void setVisualSelection (const VimContext::VisualSelection& sel, int trackIndex);

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void rebuildClipViews();

    juce::ValueTree trackState;
    juce::OwnedArray<WaveformView> clipViews;

    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;
    bool selected = false;
    int selectedClipIndex = -1;
    bool inVisualSelection = false;
    bool visualLinewise = false;
    int visualStartClip = -1;
    int visualEndClip = -1;

    static constexpr int headerWidth = 150;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLane)
};

} // namespace dc
