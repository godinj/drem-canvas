#include "BounceProcessor.h"
#include "dc/audio/AudioFileWriter.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"
#include <vector>

namespace dc
{

bool BounceProcessor::bounce (AudioGraph& graph, const BounceSettings& settings,
                              std::function<void (float progress)> progressCallback)
{
    if (settings.outputFile.empty() || settings.lengthInSamples <= 0)
        return false;

    std::filesystem::create_directories (settings.outputFile.parent_path());

    if (std::filesystem::exists (settings.outputFile))
        std::filesystem::remove (settings.outputFile);

    const int numChannels = 2;  // stereo output

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
    graph.prepare (settings.sampleRate, blockSize);

    // Allocate channel data
    std::vector<float> channelStorage (static_cast<size_t> (numChannels * blockSize), 0.0f);
    std::vector<float*> channelPtrs (static_cast<size_t> (numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        channelPtrs[static_cast<size_t> (ch)] = channelStorage.data() + ch * blockSize;

    // Empty input (offline bounce has no live input)
    std::vector<float> emptyStorage (static_cast<size_t> (numChannels * blockSize), 0.0f);
    std::vector<float*> emptyPtrs (static_cast<size_t> (numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        emptyPtrs[static_cast<size_t> (ch)] = emptyStorage.data() + ch * blockSize;

    int64_t samplesRemaining = settings.lengthInSamples;
    int64_t samplesProcessed = 0;

    while (samplesRemaining > 0)
    {
        const int samplesToProcess = static_cast<int> (
            std::min (static_cast<int64_t> (blockSize), samplesRemaining));

        dc::AudioBlock inputBlock (emptyPtrs.data(), numChannels, samplesToProcess);
        inputBlock.clear();

        dc::AudioBlock outputBlock (channelPtrs.data(), numChannels, samplesToProcess);
        outputBlock.clear();

        dc::MidiBlock midiIn;
        dc::MidiBlock midiOut;

        graph.processBlock (inputBlock, midiIn, outputBlock, midiOut, samplesToProcess);

        writer->write (outputBlock, samplesToProcess);

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
    graph.release();

    return true;
}

} // namespace dc
