#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <mutex>
#include <atomic>

namespace dc
{
namespace gfx
{

class WaveformCache
{
public:
    struct MinMaxPair
    {
        float minVal = 0.0f;
        float maxVal = 0.0f;
    };

    // LOD levels: samples per pixel bucket
    static constexpr int numLODs = 4;
    static constexpr std::array<int, numLODs> lodSamplesPerBucket = { 256, 1024, 4096, 16384 };

    struct LODData
    {
        std::vector<MinMaxPair> data;
        int samplesPerBucket = 0;
    };

    WaveformCache();
    ~WaveformCache();

    // Load from audio file (runs on background thread)
    void loadFromFile (const juce::File& audioFile, juce::AudioFormatManager& formatManager);

    // Load from existing audio buffer
    void loadFromBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate);

    // Get the best LOD for current zoom level
    const LODData* getLOD (double pixelsPerSecond, double sampleRate) const;

    // Get data for a specific region
    std::vector<MinMaxPair> getRegion (int lodIndex, int64_t startSample, int64_t numSamples) const;

    bool isLoaded() const { return loaded.load(); }
    int64_t getTotalSamples() const { return totalSamples; }

private:
    void buildLODs (const float* samples, int64_t numSamples);

    std::array<LODData, numLODs> lods;
    int64_t totalSamples = 0;
    double cachedSampleRate = 44100.0;
    std::atomic<bool> loaded { false };
    mutable std::mutex lodMutex;
};

} // namespace gfx
} // namespace dc
