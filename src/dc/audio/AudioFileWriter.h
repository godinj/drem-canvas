#pragma once

#include "AudioBlock.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
#include <sndfile.h>

namespace dc {

/// Writes audio files via libsndfile. RAII — destructor closes the file.
class AudioFileWriter
{
public:
    enum class Format
    {
        WAV_16,
        WAV_24,
        WAV_32F,
        AIFF_16,
        AIFF_24,
        FLAC_16,
        FLAC_24
    };

    /// Create a file for writing. Returns nullptr on failure.
    static std::unique_ptr<AudioFileWriter> create (
        const std::filesystem::path& path,
        Format format,
        int numChannels,
        double sampleRate);

    ~AudioFileWriter();

    /// Write interleaved samples. Returns true on success.
    bool write (const float* buffer, int64_t numFrames);

    /// Write from a multi-channel AudioBlock (interleaves internally).
    bool write (const AudioBlock& block, int numSamples);

    /// Flush and close the file.
    void close();

private:
    AudioFileWriter() = default;

    AudioFileWriter (const AudioFileWriter&) = delete;
    AudioFileWriter& operator= (const AudioFileWriter&) = delete;

    static int toSndfileFormat (Format format);

    SNDFILE* file_ = nullptr;
    SF_INFO info_{};
    int numChannels_ = 0;
    std::vector<float> interleaveBuffer_;
};

} // namespace dc
