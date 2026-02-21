#include "BounceProcessor.h"

namespace dc
{

BounceProcessor::BounceProcessor()
{
    formatManager.registerBasicFormats();
}

bool BounceProcessor::bounce (juce::AudioProcessorGraph& graph,
                              const BounceSettings& settings,
                              std::function<void (float progress)> progressCallback)
{
    if (settings.outputFile == juce::File{} || settings.lengthInSamples <= 0)
        return false;

    // Ensure the output directory exists
    settings.outputFile.getParentDirectory().createDirectory();

    // Delete any existing file so we can write fresh
    if (settings.outputFile.existsAsFile())
        settings.outputFile.deleteFile();

    auto* wavFormat = formatManager.findFormatForFileExtension ("wav");
    if (wavFormat == nullptr)
        return false;

    const int numChannels = graph.getMainBusNumOutputChannels();
    if (numChannels <= 0)
        return false;

    auto fileStream = std::make_unique<juce::FileOutputStream> (settings.outputFile);
    if (fileStream->failedToOpen())
        return false;

    std::unique_ptr<juce::OutputStream> outputStream (std::move (fileStream));

    auto options = juce::AudioFormatWriterOptions{}
                       .withSampleRate (settings.sampleRate)
                       .withNumChannels (numChannels)
                       .withBitsPerSample (settings.bitsPerSample);

    auto writer = wavFormat->createWriterFor (outputStream, options);

    if (writer == nullptr)
        return false;

    constexpr int blockSize = 512;

    // Prepare the graph for offline rendering
    graph.setPlayConfigDetails (0,                  // no inputs
                                numChannels,        // outputs
                                settings.sampleRate,
                                blockSize);
    graph.prepareToPlay (settings.sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midiBuffer;

    int64_t samplesRemaining = settings.lengthInSamples;
    int64_t samplesProcessed = 0;

    while (samplesRemaining > 0)
    {
        const int samplesToProcess = static_cast<int> (
            std::min (static_cast<int64_t> (blockSize), samplesRemaining));

        buffer.clear();
        midiBuffer.clear();

        graph.processBlock (buffer, midiBuffer);

        // Write only the samples we need (handles the final partial block)
        writer->writeFromAudioSampleBuffer (buffer, 0, samplesToProcess);

        samplesProcessed += samplesToProcess;
        samplesRemaining -= samplesToProcess;

        if (progressCallback != nullptr)
        {
            float progress = static_cast<float> (samplesProcessed)
                           / static_cast<float> (settings.lengthInSamples);
            progressCallback (progress);
        }
    }

    // Clean up the graph
    graph.releaseResources();

    return true;
}

} // namespace dc
