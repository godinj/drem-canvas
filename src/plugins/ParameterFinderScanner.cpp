#include "ParameterFinderScanner.h"
#include "vim/VimEngine.h"
#include "dc/foundation/assert.h"
#include <algorithm>
#include <map>

namespace dc
{

void ParameterFinderScanner::scan (dc::PluginInstance* plugin,
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
            int paramIdx = plugin->findParameterAtPoint (x, y);
            if (paramIdx >= 0)
            {
                unsigned int paramId = static_cast<unsigned int> (plugin->getParameterId (paramIdx));
                auto& acc = accumulators[paramId];
                acc.sumX += x;
                acc.sumY += y;
                acc.count++;
            }
        }
    }

    if (accumulators.empty())
        return;

    int numParams = plugin->getNumParameters();

    // Build results with centroids
    int mapped = 0;
    for (auto& [paramId, acc] : accumulators)
    {
        SpatialParamInfo info;
        info.paramId = paramId;
        info.centerX = static_cast<int> (acc.sumX / acc.count);
        info.centerY = static_cast<int> (acc.sumY / acc.count);
        info.hitCount = acc.count;

        // Resolve ParamID to parameter index
        for (int i = 0; i < numParams; ++i)
        {
            if (static_cast<unsigned int> (plugin->getParameterId (i)) == paramId)
            {
                info.paramIndex = i;
                info.name = plugin->getParameterName (i);
                mapped++;
                break;
            }
        }

        results.push_back (info);
    }

    // Phase 2: performEdit snoop fallback for unmapped params.
    // Drain any stale events first.
    while (plugin->popLastEdit().has_value()) {}

    int snooped = 0;
    for (auto& info : results)
    {
        if (info.paramIndex >= 0)
            continue;

        // Nudge the parameter via the finder coordinates — the plugin may call
        // performEdit, which we catch via popLastEdit().
        // This is a heuristic fallback: we re-query findParameterAtPoint and
        // check if a performEdit event was generated.
        plugin->findParameterAtPoint (info.centerX, info.centerY);

        auto edit = plugin->popLastEdit();
        if (edit.has_value())
        {
            unsigned int editId = static_cast<unsigned int> (edit->paramId);
            for (int i = 0; i < numParams; ++i)
            {
                if (static_cast<unsigned int> (plugin->getParameterId (i)) == editId)
                {
                    info.paramIndex = i;
                    info.name = plugin->getParameterName (i);
                    snooped++;
                    break;
                }
            }
        }
    }

    int unmapped = 0;
    for (auto& info : results)
    {
        if (info.paramIndex < 0)
            unmapped++;
    }

    dc_log ("[SpatialScan] %d finder params, %d direct, %d snooped, %d unmapped",
            static_cast<int> (accumulators.size()), mapped, snooped, unmapped);

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
