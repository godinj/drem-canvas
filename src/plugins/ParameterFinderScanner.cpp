#include "ParameterFinderScanner.h"
#include "VST3ParameterFinderSupport.h"
#include "vim/VimEngine.h"
#include <algorithm>
#include <map>
#include <iostream>

namespace dc
{

void ParameterFinderScanner::scan (VST3ParameterFinderSupport& finder,
                                   juce::AudioPluginInstance* plugin,
                                   int nativeWidth, int nativeHeight,
                                   int gridStep)
{
    results.clear();

    if (plugin == nullptr || nativeWidth <= 0 || nativeHeight <= 0)
        return;

    // Accumulate hit positions per ParamID
    struct Accumulator
    {
        int64_t sumX = 0;
        int64_t sumY = 0;
        int count = 0;
    };

    std::map<unsigned int, Accumulator> accumulators;

    for (int y = 0; y < nativeHeight; y += gridStep)
    {
        for (int x = 0; x < nativeWidth; x += gridStep)
        {
            unsigned int paramId = 0;
            if (finder.findParameterAt (x, y, paramId))
            {
                auto& acc = accumulators[paramId];
                acc.sumX += x;
                acc.sumY += y;
                acc.count++;
            }
        }
    }

    if (accumulators.empty())
        return;

    auto& params = plugin->getParameters();

    // Build results with centroids, resolving finder ParamIDs to JUCE indices
    // via the VST3ParameterFinderSupport interface (handles mismatched ID spaces)
    int mapped = 0;
    for (auto& [paramId, acc] : accumulators)
    {
        SpatialParamInfo info;
        info.paramId = paramId;
        info.centerX = static_cast<int> (acc.sumX / acc.count);
        info.centerY = static_cast<int> (acc.sumY / acc.count);
        info.hitCount = acc.count;

        int juceIdx = finder.resolveFinderParamIndex (paramId);
        if (juceIdx >= 0 && juceIdx < params.size())
        {
            info.juceParamIndex = juceIdx;
            info.name = params[juceIdx]->getName (64);
            mapped++;
        }
        else
        {
            info.juceParamIndex = -1;
        }

        results.push_back (info);
    }

    // Phase 2: wiggle-based fallback for unmapped params
    int wiggled = 0;
    for (auto& info : results)
    {
        if (info.juceParamIndex >= 0)
            continue;

        int idx = finder.resolveFinderParamByWiggle (info.paramId);
        if (idx >= 0 && idx < params.size())
        {
            info.juceParamIndex = idx;
            info.name = params[idx]->getName (64);
            wiggled++;
        }
    }

    // Phase 3: reverse wiggle — nudge JUCE params and detect finder param changes.
    // Useful when finder ParamIDs are outside the controller's param space.
    int reverseWiggled = 0;
    {
        std::vector<unsigned int> unmappedFinderIds;
        for (auto& info : results)
        {
            if (info.juceParamIndex < 0)
                unmappedFinderIds.push_back (info.paramId);
        }

        if (! unmappedFinderIds.empty())
        {
            std::map<unsigned int, int> reverseMap;
            int found = finder.resolveByReverseWiggle (unmappedFinderIds, reverseMap);

            if (found > 0)
            {
                for (auto& info : results)
                {
                    if (info.juceParamIndex >= 0)
                        continue;

                    auto it = reverseMap.find (info.paramId);
                    if (it != reverseMap.end() && it->second >= 0 && it->second < params.size())
                    {
                        info.juceParamIndex = it->second;
                        info.name = params[it->second]->getName (64);
                        reverseWiggled++;
                    }
                }
            }
        }
    }

    int unmapped = 0;
    for (auto& info : results)
    {
        if (info.juceParamIndex < 0)
            unmapped++;
    }

    std::cerr << "[SpatialScan] " << accumulators.size() << " finder params, "
              << mapped << " direct, "
              << wiggled << " wiggle, "
              << reverseWiggled << " reverse, "
              << unmapped << " unmapped\n";

    // Sort by position: top-to-bottom rows (20px tolerance), then left-to-right
    static constexpr int rowTolerance = 20;
    std::sort (results.begin(), results.end(),
               [] (const SpatialParamInfo& a, const SpatialParamInfo& b)
    {
        int rowA = a.centerY / rowTolerance;
        int rowB = b.centerY / rowTolerance;
        if (rowA != rowB)
            return rowA < rowB;
        return a.centerX < b.centerX;
    });

    // Assign hint labels (uniform length based on total count)
    int totalCount = static_cast<int> (results.size());
    for (int i = 0; i < totalCount; ++i)
        results[i].hintLabel = VimEngine::generateHintLabel (i, totalCount);
}

} // namespace dc
