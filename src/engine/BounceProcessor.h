#pragma once
#include "dc/engine/AudioGraph.h"
#include <filesystem>
#include <functional>

namespace dc
{

class BounceProcessor
{
public:
    BounceProcessor() = default;

    struct BounceSettings
    {
        std::filesystem::path outputFile;
        double sampleRate = 44100.0;
        int bitsPerSample = 24;
        int64_t startSample = 0;
        int64_t lengthInSamples = 0;
    };

    bool bounce (AudioGraph& graph, const BounceSettings& settings,
                 std::function<void (float progress)> progressCallback = nullptr);

private:
    BounceProcessor (const BounceProcessor&) = delete;
    BounceProcessor& operator= (const BounceProcessor&) = delete;
};

} // namespace dc
