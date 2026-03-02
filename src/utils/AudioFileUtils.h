#pragma once
#include <JuceHeader.h>
#include <filesystem>
#include <string>

namespace dc
{

class AudioFileUtils
{
public:
    AudioFileUtils();

    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    // Create a reader for an audio file (caller owns the returned pointer)
    std::unique_ptr<juce::AudioFormatReader> createReaderFor (const std::filesystem::path& file);

    // Get supported file extensions as wildcard string
    std::string getSupportedFileExtensions() const;

    // Get audio file duration in seconds
    double getFileDuration (const std::filesystem::path& file);

    // Get audio file sample rate
    double getFileSampleRate (const std::filesystem::path& file);

    // Get audio file length in samples
    int64_t getFileLengthInSamples (const std::filesystem::path& file);

private:
    juce::AudioFormatManager formatManager;

    AudioFileUtils (const AudioFileUtils&) = delete;
    AudioFileUtils& operator= (const AudioFileUtils&) = delete;
};

} // namespace dc
