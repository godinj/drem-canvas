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

    // Phase 3: Wiggle detection for still-unmapped params.
    // For plugins (e.g. Phase Plant under yabridge) whose IParameterFinder returns
    // ParamIDs in a different space from the controller's enumerable parameters,
    // we nudge the finder ParamID via setParamNormalized and detect which
    // enumerable parameter value changes.
    //
    // yabridge's setParamNormalized is fully synchronous (request-response over
    // Unix socket to Wine), so no delay is needed. If the controller doesn't
    // recognize the ParamID, it will either return kInvalidArgument or silently
    // ignore it (returning kResultOk but not changing any value).
    auto* ctrl = plugin->getController();
    int wiggled = 0;
    int wiggleDbgCount = 0;       // gate verbose logging to first 3 unmapped entries
    static constexpr int kMaxWiggleDbg = 3;

    if (ctrl != nullptr)
    {
        // Snapshot baseline values for all enumerable parameters
        std::vector<double> baseline (static_cast<size_t> (numParams));
        for (int i = 0; i < numParams; ++i)
            baseline[static_cast<size_t> (i)] =
                ctrl->getParamNormalized (plugin->getParameterId (i));

        static constexpr double kNudgeAmount = 0.002;
        static constexpr double kDetectThreshold = 0.0005;

        for (auto& info : results)
        {
            if (info.paramIndex >= 0)
                continue;

            bool verbose = (wiggleDbgCount < kMaxWiggleDbg);
            wiggleDbgCount++;

            auto finderParamId = static_cast<Steinberg::Vst::ParamID> (info.paramId);

            // Read the current value of this finder ParamID
            double original = ctrl->getParamNormalized (finderParamId);

            // Compute nudge direction: nudge down if near 1.0, else up
            double nudged = (original > 0.998) ? original - kNudgeAmount
                                                : original + kNudgeAmount;

            if (verbose)
            {
                dc_log ("[WiggleDbg] entry #%d: finderParamId=%u  original=%.6f  nudged=%.6f",
                        wiggleDbgCount, static_cast<unsigned int> (finderParamId), original, nudged);
            }

            // Apply the nudge
            auto setResult = ctrl->setParamNormalized (finderParamId, nudged);

            if (setResult != Steinberg::kResultOk)
            {
                if (verbose)
                    dc_log ("[WiggleDbg]   setParamNormalized failed: %d", static_cast<int> (setResult));
                continue;
            }

            // Re-read the finder ParamID to confirm the set actually took effect
            if (verbose)
            {
                double afterSet = ctrl->getParamNormalized (finderParamId);
                dc_log ("[WiggleDbg]   after setParamNormalized: re-read finderParamId=%u => %.6f  (delta=%.6f)",
                        static_cast<unsigned int> (finderParamId), afterSet, afterSet - original);
            }

            // Check which enumerable parameter changed
            int matchIdx = -1;
            int changesLogged = 0;

            for (int i = 0; i < numParams; ++i)
            {
                double current = ctrl->getParamNormalized (plugin->getParameterId (i));
                double delta = current - baseline[static_cast<size_t> (i)];

                if (std::abs (delta) > 0.0)
                {
                    if (verbose && changesLogged < 5)
                    {
                        dc_log ("[WiggleDbg]   enum param[%d] id=%u changed: baseline=%.6f current=%.6f delta=%.6f %s",
                                i, static_cast<unsigned int> (plugin->getParameterId (i)),
                                baseline[static_cast<size_t> (i)], current, delta,
                                (std::abs (delta) > kDetectThreshold) ? "ABOVE_THRESHOLD" : "below_threshold");
                        changesLogged++;
                    }

                    if (matchIdx < 0 && std::abs (delta) > kDetectThreshold)
                        matchIdx = i;
                }
            }

            if (verbose)
            {
                if (changesLogged == 0)
                    dc_log ("[WiggleDbg]   NO enumerable params changed at all (0 deltas)");

                dc_log ("[WiggleDbg]   matchIdx=%d", matchIdx);
            }

            // Restore original value
            ctrl->setParamNormalized (finderParamId, original);

            // Verify restore took effect
            if (verbose)
            {
                double afterRestore = ctrl->getParamNormalized (finderParamId);
                dc_log ("[WiggleDbg]   after restore: re-read finderParamId=%u => %.6f  (expected %.6f)",
                        static_cast<unsigned int> (finderParamId), afterRestore, original);
            }

            if (matchIdx >= 0)
            {
                info.paramIndex = matchIdx;
                info.name = plugin->getParameterName (matchIdx);
                wiggled++;

                dc_log ("[WiggleDetect] finderParam=%u -> enumIndex=%d name=\"%s\"",
                        info.paramId, matchIdx, info.name.c_str());

                // Update baseline for the matched param (in case of rounding)
                baseline[static_cast<size_t> (matchIdx)] =
                    ctrl->getParamNormalized (plugin->getParameterId (matchIdx));
            }
        }
    }
    else
    {
        dc_log ("[WiggleDbg] ctrl is null — skipping wiggle phase");
    }

    int unmapped = 0;
    for (auto& info : results)
    {
        if (info.paramIndex < 0)
            unmapped++;
    }

    dc_log ("[SpatialScan] %d finder hits, %d after filter (min %d), %d direct, %d snooped, %d wiggled, %d unmapped",
            preFilterCount, static_cast<int> (results.size()), minHitCount, mapped, snooped, wiggled, unmapped);

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
