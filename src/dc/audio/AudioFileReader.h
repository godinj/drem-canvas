#pragma once

#include "AudioBlock.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <sndfile.h>

namespace dc {

/// Reads audio files via libsndfile. RAII — destructor closes the file.
class AudioFileReader
{
public:
    /// Open a file for reading. Returns nullptr on failure.
    static std::unique_ptr<AudioFileReader> open (const std::filesystem::path& path);

    ~AudioFileReader();

    int getNumChannels() const;
    int64_t getLengthInSamples() const;
    double getSampleRate() const;

    /// Read interleaved samples into buffer.
    /// Returns number of frames actually read.
    int64_t read (float* buffer, int64_t startFrame, int64_t numFrames);

    /// Read into a multi-channel AudioBlock.
    /// De-interleaves into separate channel buffers.
    int64_t read (AudioBlock& block, int64_t startFrame, int64_t numFrames);

    /// Get file format info
    std::string getFormatName() const;
    int getBitDepth() const;

private:
    AudioFileReader() = default;

    AudioFileReader (const AudioFileReader&) = delete;
    AudioFileReader& operator= (const AudioFileReader&) = delete;

    SNDFILE* file_ = nullptr;
    SF_INFO info_{};
    std::filesystem::path path_;
};

} // namespace dc
