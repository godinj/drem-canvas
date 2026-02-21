#pragma once
#include <JuceHeader.h>

namespace dc
{

class AudioFileUtils
{
public:
    AudioFileUtils();

    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    // Create a reader for an audio file (caller owns the returned pointer)
    std::unique_ptr<juce::AudioFormatReader> createReaderFor (const juce::File& file);

    // Get supported file extensions as wildcard string
    juce::String getSupportedFileExtensions() const;

    // Get audio file duration in seconds
    double getFileDuration (const juce::File& file);

    // Get audio file sample rate
    double getFileSampleRate (const juce::File& file);

    // Get audio file length in samples
    int64_t getFileLengthInSamples (const juce::File& file);

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileUtils)
};

} // namespace dc
