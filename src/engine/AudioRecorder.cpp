#include "AudioRecorder.h"

namespace dc
{

AudioRecorder::AudioRecorder()
{
    formatManager.registerBasicFormats();
    writerThread.startThread (juce::Thread::Priority::normal);
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
    writerThread.stopThread (1000);
}

bool AudioRecorder::startRecording (const juce::File& outputFile, double sampleRate,
                                    int numChannels, int bitsPerSample)
{
    stopRecording();

    if (outputFile == juce::File{})
        return false;

    // Ensure the parent directory exists
    outputFile.getParentDirectory().createDirectory();

    // Delete existing file so the writer can create a fresh one
    if (outputFile.existsAsFile())
        outputFile.deleteFile();

    auto* wavFormat = formatManager.findFormatForFileExtension ("wav");
    if (wavFormat == nullptr)
        return false;

    auto fileStream = std::make_unique<juce::FileOutputStream> (outputFile);
    if (fileStream->failedToOpen())
        return false;

    std::unique_ptr<juce::OutputStream> outputStream (std::move (fileStream));

    auto options = juce::AudioFormatWriterOptions{}
                       .withSampleRate (sampleRate)
                       .withNumChannels (numChannels)
                       .withBitsPerSample (bitsPerSample);

    auto writer = wavFormat->createWriterFor (outputStream, options);

    if (writer == nullptr)
        return false;

    // Wrap in a ThreadedWriter for lock-free writing from the audio thread.
    // Buffer size of 48000 samples gives roughly 1 second of buffering at 48 kHz.
    constexpr int bufferSize = 48000;
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (
        writer.release(), writerThread, bufferSize);

    recordedFile = outputFile;
    recordedSamples.store (0);
    recording.store (true);

    return true;
}

void AudioRecorder::stopRecording()
{
    recording.store (false);

    // Reset the threaded writer which flushes remaining samples and
    // closes the underlying file writer.
    threadedWriter.reset();
}

void AudioRecorder::writeAudioBlock (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (! recording.load())
        return;

    if (threadedWriter != nullptr)
    {
        // ThreadedWriter::write expects a pointer array and a sample count.
        threadedWriter->write (buffer.getArrayOfReadPointers(), numSamples);
        recordedSamples.fetch_add (static_cast<int64_t> (numSamples));
    }
}

} // namespace dc
