#pragma once

#include "dc/plugins/PluginInstance.h"
#include <string>
#include <vector>

namespace dc
{

/** Spatial info for a single parameter discovered via IParameterFinder grid scan. */
struct SpatialParamInfo
{
    unsigned int paramId = 0;   // VST3 ParamID
    int paramIndex = -1;        // Index into plugin parameters, -1 if unmapped
    int centerX = 0;            // Centroid X in plugin native coords (unscaled)
    int centerY = 0;            // Centroid Y in plugin native coords (unscaled)
    int hitCount = 0;           // Grid cells that hit this param
    std::string name;           // Cached parameter name
    std::string hintLabel;      // Assigned hint label (a, s, d, ...)
};

/**
 * Scans a plugin editor via IParameterFinder to build a spatial map
 * of parameter locations with assigned hint labels.
 */
class ParameterFinderScanner
{
public:
    ParameterFinderScanner() = default;

    /** Run a grid scan of the editor surface.
        @param plugin       The dc::PluginInstance (provides findParameterAtPoint/popLastEdit)
        @param nativeWidth  Editor width in native (unscaled) pixels
        @param nativeHeight Editor height in native (unscaled) pixels
        @param gridStep     Scan interval in pixels (default 8)
    */
    void scan (dc::PluginInstance* plugin,
               int nativeWidth, int nativeHeight,
               int gridStep = 8);

    bool hasResults() const { return ! results.empty(); }
    const std::vector<SpatialParamInfo>& getResults() const { return results; }
    std::vector<SpatialParamInfo>& getMutableResults() { return results; }
    void clear() { results.clear(); }

private:
    std::vector<SpatialParamInfo> results;

    ParameterFinderScanner (const ParameterFinderScanner&) = delete;
    ParameterFinderScanner& operator= (const ParameterFinderScanner&) = delete;
};

} // namespace dc
