#include "ParameterFinderScanner.h"
#include "vim/VimEngine.h"
#include "dc/foundation/assert.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace dc
{

void ParameterFinderScanner::scan (dc::PluginInstance* plugin,
                                   int nativeWidth, int nativeHeight,
                                   int gridStep)
{
    results.clear();

    if (plugin == nullptr || nativeWidth <= 0 || nativeHeight <= 0)
        return;

    // Accumulate hit positions per raw finder ParamID.
    // Using raw ParamIDs (instead of resolved parameter indices) ensures that
    // plugins with disjoint finder/enumerable ID spaces (e.g. Phase Plant)
    // still produce spatial centroids from the grid scan.
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
            auto rawId = plugin->findRawParameterAtPoint (x, y);
            if (rawId != Steinberg::Vst::kNoParamId)
            {
                auto& acc = accumulators[static_cast<unsigned int> (rawId)];
                acc.sumX += x;
                acc.sumY += y;
                acc.count++;
            }
        }
    }

    if (accumulators.empty())
        return;

    int numParams = plugin->getNumParameters();

    // Phase 1: Build results with centroids. Try direct ParamID-to-index mapping.
    int mapped = 0;
    for (auto& [paramId, acc] : accumulators)
    {
        SpatialParamInfo info;
        info.paramId = paramId;
        info.centerX = static_cast<int> (acc.sumX / acc.count);
        info.centerY = static_cast<int> (acc.sumY / acc.count);
        info.hitCount = acc.count;

        // Direct lookup: resolve raw finder ParamID to enumerable parameter index
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

    // Filter: remove params with fewer than minHitCount grid cells.
    // With gridStep=8, 3 cells ≈ a 24x24 pixel control — below this is
    // noise from edge hits, invisible overlays, or hidden-tab parameters.
    static constexpr int minHitCount = 3;
    int preFilterCount = static_cast<int> (results.size());
    results.erase (
        std::remove_if (results.begin(), results.end(),
            [] (const SpatialParamInfo& info) { return info.hitCount < minHitCount; }),
        results.end());

    // Phase 2: performEdit snoop fallback for unmapped params.
    // Re-query findParameterAtPoint at each unmapped param's centroid. Some plugins
    // call performEdit in response to IParameterFinder queries.
    // Drain any stale events first.
    while (plugin->popLastEdit().has_value()) {}

    int snooped = 0;
    for (auto& info : results)
    {
        if (info.paramIndex >= 0)
            continue;

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

    // Remaining unmapped entries are deferred to mouse probe resolution
    // in PluginViewWidget::runSpatialScan() Phase 4, which injects synthetic
    // mouse events at centroid positions and catches performEdit callbacks.
    // This is more reliable than setParamNormalized wiggle for plugins with
    // disjoint finder ParamIDs (e.g. Phase Plant under yabridge).
    int unmapped = 0;
    for (auto& info : results)
    {
        if (info.paramIndex < 0)
            unmapped++;
    }

    dc_log ("[SpatialScan] %d finder hits, %d after filter (min %d), %d direct, %d snooped, %d unmapped (deferred to mouse probe)",
            preFilterCount, static_cast<int> (results.size()), minHitCount, mapped, snooped, unmapped);

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
