#pragma once
#include "dc/audio/ThreadedRecorder.h"
#include "dc/audio/AudioBlock.h"
#include <atomic>
#include <filesystem>
#include <memory>

namespace dc
{

class AudioRecorder
{
public:
    AudioRecorder();
    ~AudioRecorder();

    // Start recording to a file
    bool startRecording (const std::filesystem::path& outputFile, double sampleRate,
                         int numChannels = 2, int bitsPerSample = 24);
    void stopRecording();
    bool isRecording() const { return recording.load(); }

    // Call from audio callback to feed samples
    void writeAudioBlock (const dc::AudioBlock& block, int numSamples);

    std::filesystem::path getRecordedFile() const { return recordedFile; }
    int64_t getRecordedSampleCount() const { return recordedSamples.load(); }

private:
    std::unique_ptr<dc::ThreadedRecorder> recorder;

    std::atomic<bool> recording { false };
    std::atomic<int64_t> recordedSamples { 0 };
    std::filesystem::path recordedFile;

    AudioRecorder (const AudioRecorder&) = delete;
    AudioRecorder& operator= (const AudioRecorder&) = delete;
};

} // namespace dc
