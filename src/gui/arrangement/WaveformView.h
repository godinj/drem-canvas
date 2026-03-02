#pragma once
#include <JuceHeader.h>
#include <filesystem>
#include "dc/foundation/types.h"

namespace dc
{

class WaveformView : public juce::Component,
                     private juce::ChangeListener
{
public:
    WaveformView();
    ~WaveformView() override;

    void setFile (const std::filesystem::path& file);
    void setWaveformColour (dc::Colour c) { waveformColour = c; }

    void paint (juce::Graphics& g) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
    dc::Colour waveformColour { dc::Colours::cyan };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

} // namespace dc
