#include "BounceProcessor.h"
#include "dc/audio/AudioFileWriter.h"
#include "dc/audio/AudioBlock.h"
#include "dc/midi/MidiBuffer.h"
#include "MidiBridge.h"
#include <vector>

namespace dc
{

bool BounceProcessor::bounce (juce::AudioProcessorGraph& graph,
                              const BounceSettings& settings,
                              std::function<void (float progress)> progressCallback)
{
    if (settings.outputFile.empty() || settings.lengthInSamples <= 0)
        return false;

    // Ensure the output directory exists
    std::filesystem::create_directories (settings.outputFile.parent_path());

    // Delete any existing file so we can write fresh
    if (std::filesystem::exists (settings.outputFile))
        std::filesystem::remove (settings.outputFile);

    const int numChannels = graph.getMainBusNumOutputChannels();
    if (numChannels <= 0)
        return false;

    // Map bitsPerSample to dc::AudioFileWriter::Format
    AudioFileWriter::Format format;
    switch (settings.bitsPerSample)
    {
        case 16: format = AudioFileWriter::Format::WAV_16; break;
        case 32: format = AudioFileWriter::Format::WAV_32F; break;
        default: format = AudioFileWriter::Format::WAV_24; break;
    }

    auto writer = AudioFileWriter::create (settings.outputFile, format,
                                           numChannels, settings.sampleRate);
    if (writer == nullptr)
        return false;

    constexpr int blockSize = 512;

    // Prepare the graph for offline rendering
    graph.setPlayConfigDetails (0,                  // no inputs
                                numChannels,        // outputs
                                settings.sampleRate,
                                blockSize);
    graph.prepareToPlay (settings.sampleRate, blockSize);

    // Allocate channel data — dc::AudioBlock is the primary representation
    std::vector<float> channelStorage (static_cast<size_t> (numChannels * blockSize), 0.0f);
    std::vector<float*> channelPtrs (static_cast<size_t> (numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        channelPtrs[static_cast<size_t> (ch)] = channelStorage.data() + ch * blockSize;

    int64_t samplesRemaining = settings.lengthInSamples;
    int64_t samplesProcessed = 0;

    while (samplesRemaining > 0)
    {
        const int samplesToProcess = static_cast<int> (
            std::min (static_cast<int64_t> (blockSize), samplesRemaining));

        dc::AudioBlock block (channelPtrs.data(), numChannels, blockSize);
        block.clear();

        dc::MidiBuffer dcMidi;

        // Wrap to juce types at the graph.processBlock() boundary (Phase 3)
        juce::AudioBuffer<float> juceBuffer (channelPtrs.data(), numChannels, blockSize);
        juce::MidiBuffer juceMidi;
        graph.processBlock (juceBuffer, juceMidi);

        // Write using dc::AudioBlock
        dc::AudioBlock outBlock (channelPtrs.data(), numChannels, samplesToProcess);
        writer->write (outBlock, samplesToProcess);

        samplesProcessed += samplesToProcess;
        samplesRemaining -= samplesToProcess;

        if (progressCallback != nullptr)
        {
            float progress = static_cast<float> (samplesProcessed)
                           / static_cast<float> (settings.lengthInSamples);
            progressCallback (progress);
        }
    }

    writer->close();

    // Clean up the graph
    graph.releaseResources();

    return true;
}

} // namespace dc
