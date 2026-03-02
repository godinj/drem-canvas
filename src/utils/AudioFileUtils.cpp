#include "AudioFileUtils.h"

namespace dc
{

AudioFileUtils::AudioFileUtils()
{
    formatManager.registerBasicFormats();
}

std::unique_ptr<juce::AudioFormatReader> AudioFileUtils::createReaderFor (const std::filesystem::path& file)
{
    return std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (juce::File (file.string())));
}

std::string AudioFileUtils::getSupportedFileExtensions() const
{
    return formatManager.getWildcardForAllFormats().toStdString();
}

double AudioFileUtils::getFileDuration (const std::filesystem::path& file)
{
    auto reader = createReaderFor (file);

    if (reader != nullptr)
        return static_cast<double> (reader->lengthInSamples) / reader->sampleRate;

    return 0.0;
}

double AudioFileUtils::getFileSampleRate (const std::filesystem::path& file)
{
    auto reader = createReaderFor (file);

    if (reader != nullptr)
        return reader->sampleRate;

    return 0.0;
}

int64_t AudioFileUtils::getFileLengthInSamples (const std::filesystem::path& file)
{
    auto reader = createReaderFor (file);

    if (reader != nullptr)
        return reader->lengthInSamples;

    return 0;
}

} // namespace dc
