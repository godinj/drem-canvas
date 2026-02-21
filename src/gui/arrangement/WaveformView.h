#pragma once
#include <JuceHeader.h>

namespace dc
{

class WaveformView : public juce::Component,
                     private juce::ChangeListener
{
public:
    WaveformView();
    ~WaveformView() override;

    void setFile (const juce::File& file);
    void setWaveformColour (juce::Colour c) { waveformColour = c; }

    void paint (juce::Graphics& g) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
    juce::Colour waveformColour { juce::Colours::cyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

} // namespace dc
