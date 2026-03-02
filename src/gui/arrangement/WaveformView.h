#pragma once
#include <JuceHeader.h>
#include <filesystem>
#include "dc/foundation/types.h"
#include "graphics/rendering/WaveformCache.h"

namespace dc
{

class WaveformView : public juce::Component
{
public:
    WaveformView();
    ~WaveformView() override;

    void setFile (const std::filesystem::path& file);
    void setWaveformColour (dc::Colour c) { waveformColour = c; }
    void setSampleRate (double sr) { sampleRate = sr; }

    void paint (juce::Graphics& g) override;

private:
    dc::gfx::WaveformCache waveformCache;
    dc::Colour waveformColour { dc::Colours::cyan };
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

} // namespace dc
