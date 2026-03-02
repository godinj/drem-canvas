#include "AudioRecorder.h"
#include "dc/audio/AudioFileWriter.h"
#include <filesystem>

namespace dc
{

AudioRecorder::AudioRecorder()
{
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
}

bool AudioRecorder::startRecording (const std::filesystem::path& outputFile, double sampleRate,
                                    int numChannels, int bitsPerSample)
{
    stopRecording();

    if (outputFile.empty())
        return false;

    // Ensure the parent directory exists
    std::filesystem::create_directories (outputFile.parent_path());

    // Delete existing file so the writer can create a fresh one
    if (std::filesystem::exists (outputFile))
        std::filesystem::remove (outputFile);

    // Map bitsPerSample to dc::AudioFileWriter::Format
    dc::AudioFileWriter::Format format;
    switch (bitsPerSample)
    {
        case 16: format = dc::AudioFileWriter::Format::WAV_16; break;
        case 32: format = dc::AudioFileWriter::Format::WAV_32F; break;
        default: format = dc::AudioFileWriter::Format::WAV_24; break;
    }

    recorder = std::make_unique<dc::ThreadedRecorder>();

    if (! recorder->start (outputFile, format, numChannels, sampleRate))
    {
        recorder.reset();
        return false;
    }

    recordedFile = outputFile;
    recordedSamples.store (0);
    recording.store (true);

    return true;
}

void AudioRecorder::stopRecording()
{
    recording.store (false);

    if (recorder)
    {
        recorder->stop();
        recorder.reset();
    }
}

void AudioRecorder::writeAudioBlock (const dc::AudioBlock& block, int numSamples)
{
    if (! recording.load())
        return;

    if (recorder)
    {
        recorder->write (block, numSamples);
        recordedSamples.fetch_add (static_cast<int64_t> (numSamples));
    }
}

} // namespace dc
