#include "AudioFileReader.h"
#include <algorithm>
#include <vector>

namespace dc {

std::unique_ptr<AudioFileReader> AudioFileReader::open (const std::filesystem::path& path)
{
    std::unique_ptr<AudioFileReader> reader (new AudioFileReader());
    reader->path_ = path;
    reader->info_ = {};

    reader->file_ = sf_open (path.string().c_str(), SFM_READ, &reader->info_);
    if (reader->file_ == nullptr)
        return nullptr;

    return reader;
}

AudioFileReader::~AudioFileReader()
{
    if (file_ != nullptr)
    {
        sf_close (file_);
        file_ = nullptr;
    }
}

int AudioFileReader::getNumChannels() const
{
    return info_.channels;
}

int64_t AudioFileReader::getLengthInSamples() const
{
    return info_.frames;
}

double AudioFileReader::getSampleRate() const
{
    return static_cast<double> (info_.samplerate);
}

int64_t AudioFileReader::read (float* buffer, int64_t startFrame, int64_t numFrames)
{
    if (file_ == nullptr)
        return 0;

    sf_seek (file_, startFrame, SEEK_SET);
    return sf_readf_float (file_, buffer, numFrames);
}

int64_t AudioFileReader::read (AudioBlock& block, int64_t startFrame, int64_t numFrames)
{
    if (file_ == nullptr)
        return 0;

    int channels = info_.channels;
    int64_t framesToRead = std::min (numFrames, static_cast<int64_t> (block.getNumSamples()));

    // Read interleaved into temporary buffer
    std::vector<float> interleaved (static_cast<size_t> (framesToRead * channels));
    sf_seek (file_, startFrame, SEEK_SET);
    int64_t framesRead = sf_readf_float (file_, interleaved.data(), framesToRead);

    // De-interleave into block channels
    int blockChannels = std::min (channels, block.getNumChannels());
    for (int64_t f = 0; f < framesRead; ++f)
    {
        for (int ch = 0; ch < blockChannels; ++ch)
            block.getChannel (ch)[f] = interleaved[static_cast<size_t> (f * channels + ch)];
    }

    return framesRead;
}

std::string AudioFileReader::getFormatName() const
{
    int majorFormat = info_.format & SF_FORMAT_TYPEMASK;

    switch (majorFormat)
    {
        case SF_FORMAT_WAV:   return "WAV";
        case SF_FORMAT_AIFF:  return "AIFF";
        case SF_FORMAT_FLAC:  return "FLAC";
        case SF_FORMAT_OGG:   return "OGG";
        case SF_FORMAT_AU:    return "AU";
        case SF_FORMAT_RAW:   return "RAW";
        case SF_FORMAT_CAF:   return "CAF";
        case SF_FORMAT_W64:   return "W64";
        case SF_FORMAT_RF64:  return "RF64";
        default:              return "Unknown";
    }
}

int AudioFileReader::getBitDepth() const
{
    int subFormat = info_.format & SF_FORMAT_SUBMASK;

    switch (subFormat)
    {
        case SF_FORMAT_PCM_S8:
        case SF_FORMAT_PCM_U8:   return 8;
        case SF_FORMAT_PCM_16:   return 16;
        case SF_FORMAT_PCM_24:   return 24;
        case SF_FORMAT_PCM_32:
        case SF_FORMAT_FLOAT:    return 32;
        case SF_FORMAT_DOUBLE:   return 64;
        default:                 return 0;
    }
}

} // namespace dc
