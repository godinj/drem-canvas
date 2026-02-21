#pragma once
#include <JuceHeader.h>

namespace dc
{

class BounceProcessor
{
public:
    BounceProcessor();

    struct BounceSettings
    {
        juce::File outputFile;
        double sampleRate = 44100.0;
        int bitsPerSample = 24;
        int64_t startSample = 0;
        int64_t lengthInSamples = 0;
    };

    // Bounce the given processor graph to a file
    bool bounce (juce::AudioProcessorGraph& graph,
                 const BounceSettings& settings,
                 std::function<void (float progress)> progressCallback = nullptr);

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BounceProcessor)
};

} // namespace dc
