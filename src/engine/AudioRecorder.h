#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace dc
{

class AudioRecorder
{
public:
    AudioRecorder();
    ~AudioRecorder();

    // Start recording to a file
    bool startRecording (const juce::File& outputFile, double sampleRate,
                         int numChannels = 2, int bitsPerSample = 24);
    void stopRecording();
    bool isRecording() const { return recording.load(); }

    // Call from audio callback to feed samples
    void writeAudioBlock (const juce::AudioBuffer<float>& buffer, int numSamples);

    juce::File getRecordedFile() const { return recordedFile; }
    int64_t getRecordedSampleCount() const { return recordedSamples.load(); }

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::TimeSliceThread writerThread { "AudioRecorderWriter" };

    std::atomic<bool> recording { false };
    std::atomic<int64_t> recordedSamples { 0 };
    juce::File recordedFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecorder)
};

} // namespace dc
