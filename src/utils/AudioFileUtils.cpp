#include "AudioFileUtils.h"
#include "dc/audio/AudioFileReader.h"

namespace dc
{

std::string AudioFileUtils::getSupportedFileExtensions() const
{
    return "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.caf;*.w64;*.rf64";
}

double AudioFileUtils::getFileDuration (const std::filesystem::path& file)
{
    auto reader = AudioFileReader::open (file);

    if (reader != nullptr)
        return static_cast<double> (reader->getLengthInSamples()) / reader->getSampleRate();

    return 0.0;
}

double AudioFileUtils::getFileSampleRate (const std::filesystem::path& file)
{
    auto reader = AudioFileReader::open (file);

    if (reader != nullptr)
        return reader->getSampleRate();

    return 0.0;
}

int64_t AudioFileUtils::getFileLengthInSamples (const std::filesystem::path& file)
{
    auto reader = AudioFileReader::open (file);

    if (reader != nullptr)
        return reader->getLengthInSamples();

    return 0;
}

} // namespace dc
