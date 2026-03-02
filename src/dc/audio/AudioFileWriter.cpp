#include "AudioFileWriter.h"

namespace dc {

int AudioFileWriter::toSndfileFormat (Format format)
{
    switch (format)
    {
        case Format::WAV_16:   return SF_FORMAT_WAV  | SF_FORMAT_PCM_16;
        case Format::WAV_24:   return SF_FORMAT_WAV  | SF_FORMAT_PCM_24;
        case Format::WAV_32F:  return SF_FORMAT_WAV  | SF_FORMAT_FLOAT;
        case Format::AIFF_16:  return SF_FORMAT_AIFF | SF_FORMAT_PCM_16;
        case Format::AIFF_24:  return SF_FORMAT_AIFF | SF_FORMAT_PCM_24;
        case Format::FLAC_16:  return SF_FORMAT_FLAC | SF_FORMAT_PCM_16;
        case Format::FLAC_24:  return SF_FORMAT_FLAC | SF_FORMAT_PCM_24;
    }

    return SF_FORMAT_WAV | SF_FORMAT_PCM_24;  // fallback
}

std::unique_ptr<AudioFileWriter> AudioFileWriter::create (
    const std::filesystem::path& path,
    Format format,
    int numChannels,
    double sampleRate)
{
    std::unique_ptr<AudioFileWriter> writer (new AudioFileWriter());
    writer->numChannels_ = numChannels;

    writer->info_ = {};
    writer->info_.samplerate = static_cast<int> (sampleRate);
    writer->info_.channels = numChannels;
    writer->info_.format = toSndfileFormat (format);

    writer->file_ = sf_open (path.string().c_str(), SFM_WRITE, &writer->info_);
    if (writer->file_ == nullptr)
        return nullptr;

    return writer;
}

AudioFileWriter::~AudioFileWriter()
{
    close();
}

bool AudioFileWriter::write (const float* buffer, int64_t numFrames)
{
    if (file_ == nullptr)
        return false;

    int64_t written = sf_writef_float (file_, buffer, numFrames);
    return written == numFrames;
}

bool AudioFileWriter::write (const AudioBlock& block, int numSamples)
{
    if (file_ == nullptr)
        return false;

    int channels = numChannels_;
    interleaveBuffer_.resize (static_cast<size_t> (numSamples * channels));

    for (int f = 0; f < numSamples; ++f)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            int blockCh = (ch < block.getNumChannels()) ? ch : 0;
            interleaveBuffer_[static_cast<size_t> (f * channels + ch)] =
                block.getChannel (blockCh)[f];
        }
    }

    int64_t written = sf_writef_float (file_, interleaveBuffer_.data(), numSamples);
    return written == numSamples;
}

void AudioFileWriter::close()
{
    if (file_ != nullptr)
    {
        sf_close (file_);
        file_ = nullptr;
    }
}

} // namespace dc
