#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace dc
{

class AudioFileUtils
{
public:
    AudioFileUtils() = default;

    // Get supported file extensions as wildcard string
    std::string getSupportedFileExtensions() const;

    // Get audio file duration in seconds
    double getFileDuration (const std::filesystem::path& file);

    // Get audio file sample rate
    double getFileSampleRate (const std::filesystem::path& file);

    // Get audio file length in samples
    int64_t getFileLengthInSamples (const std::filesystem::path& file);

private:
    AudioFileUtils (const AudioFileUtils&) = delete;
    AudioFileUtils& operator= (const AudioFileUtils&) = delete;
};

} // namespace dc
