#include "WaveformCache.h"
#include <cmath>
#include <algorithm>

namespace dc
{
namespace gfx
{

WaveformCache::WaveformCache()
{
}

WaveformCache::~WaveformCache()
{
}

void WaveformCache::loadFromFile (const juce::File& audioFile, juce::AudioFormatManager& formatManager)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
    if (!reader)
        return;

    int64_t numSamples = static_cast<int64_t> (reader->lengthInSamples);
    cachedSampleRate = reader->sampleRate;

    // Read into temporary buffer (channel 0 only for waveform display)
    juce::AudioBuffer<float> buffer (1, static_cast<int> (std::min (numSamples, (int64_t) 10000000)));
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, false);

    buildLODs (buffer.getReadPointer (0), buffer.getNumSamples());
}

void WaveformCache::loadFromBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    cachedSampleRate = sampleRate;
    if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
        buildLODs (buffer.getReadPointer (0), buffer.getNumSamples());
}

void WaveformCache::buildLODs (const float* samples, int64_t numSamples)
{
    std::lock_guard<std::mutex> lock (lodMutex);

    totalSamples = numSamples;

    for (int lod = 0; lod < numLODs; ++lod)
    {
        int spb = lodSamplesPerBucket[static_cast<size_t> (lod)];
        int64_t numBuckets = (numSamples + spb - 1) / spb;

        lods[static_cast<size_t> (lod)].samplesPerBucket = spb;
        lods[static_cast<size_t> (lod)].data.resize (static_cast<size_t> (numBuckets));

        for (int64_t b = 0; b < numBuckets; ++b)
        {
            int64_t start = b * spb;
            int64_t end = std::min (start + spb, numSamples);

            float minVal = 0.0f;
            float maxVal = 0.0f;

            for (int64_t s = start; s < end; ++s)
            {
                float v = samples[s];
                minVal = std::min (minVal, v);
                maxVal = std::max (maxVal, v);
            }

            lods[static_cast<size_t> (lod)].data[static_cast<size_t> (b)] = { minVal, maxVal };
        }
    }

    loaded.store (true);
}

const WaveformCache::LODData* WaveformCache::getLOD (double pixelsPerSecond, double sampleRate) const
{
    if (!loaded.load())
        return nullptr;

    // Calculate samples per pixel at current zoom
    double samplesPerPixel = sampleRate / pixelsPerSecond;

    // Find the best LOD (the one with samplesPerBucket closest to but <= samplesPerPixel)
    int bestLOD = 0;
    for (int i = numLODs - 1; i >= 0; --i)
    {
        if (lodSamplesPerBucket[static_cast<size_t> (i)] <= samplesPerPixel)
        {
            bestLOD = i;
            break;
        }
    }

    return &lods[static_cast<size_t> (bestLOD)];
}

std::vector<WaveformCache::MinMaxPair> WaveformCache::getRegion (
    int lodIndex, int64_t startSample, int64_t numSamples) const
{
    std::lock_guard<std::mutex> lock (lodMutex);

    if (lodIndex < 0 || lodIndex >= numLODs)
        return {};

    const auto& lod = lods[static_cast<size_t> (lodIndex)];
    int spb = lod.samplesPerBucket;

    int64_t startBucket = startSample / spb;
    int64_t endBucket = (startSample + numSamples + spb - 1) / spb;
    int64_t numBuckets = static_cast<int64_t> (lod.data.size());

    startBucket = std::clamp (startBucket, (int64_t) 0, numBuckets);
    endBucket = std::clamp (endBucket, (int64_t) 0, numBuckets);

    if (startBucket >= endBucket)
        return {};

    return std::vector<MinMaxPair> (
        lod.data.begin() + startBucket,
        lod.data.begin() + endBucket);
}

} // namespace gfx
} // namespace dc
