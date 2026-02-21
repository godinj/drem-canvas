#include "AudioFileUtils.h"

namespace dc
{

AudioFileUtils::AudioFileUtils()
{
    formatManager.registerBasicFormats();
}

std::unique_ptr<juce::AudioFormatReader> AudioFileUtils::createReaderFor (const juce::File& file)
{
    return std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (file));
}

juce::String AudioFileUtils::getSupportedFileExtensions() const
{
    return formatManager.getWildcardForAllFormats();
}

double AudioFileUtils::getFileDuration (const juce::File& file)
{
    auto reader = createReaderFor (file);

    if (reader != nullptr)
        return static_cast<double> (reader->lengthInSamples) / reader->sampleRate;

    return 0.0;
}

double AudioFileUtils::getFileSampleRate (const juce::File& file)
{
    auto reader = createReaderFor (file);

    if (reader != nullptr)
        return reader->sampleRate;

    return 0.0;
}

int64_t AudioFileUtils::getFileLengthInSamples (const juce::File& file)
{
    auto reader = createReaderFor (file);

    if (reader != nullptr)
        return reader->lengthInSamples;

    return 0;
}

} // namespace dc
