#pragma once
#include <JuceHeader.h>
#include <filesystem>

namespace dc
{

class BounceProcessor
{
public:
    BounceProcessor();

    struct BounceSettings
    {
        std::filesystem::path outputFile;
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

    BounceProcessor (const BounceProcessor&) = delete;
    BounceProcessor& operator= (const BounceProcessor&) = delete;
};

} // namespace dc
