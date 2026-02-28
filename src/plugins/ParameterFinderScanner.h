#pragma once

#include <JuceHeader.h>
#include <vector>

namespace dc
{

class VST3ParameterFinderSupport;

/** Spatial info for a single parameter discovered via IParameterFinder grid scan. */
struct SpatialParamInfo
{
    unsigned int paramId = 0;   // VST3 ParamID
    int juceParamIndex = -1;    // Index into plugin->getParameters(), -1 if unmapped
    int centerX = 0;            // Centroid X in plugin native coords (unscaled)
    int centerY = 0;            // Centroid Y in plugin native coords (unscaled)
    int hitCount = 0;           // Grid cells that hit this param
    juce::String name;          // Cached parameter name
    juce::String hintLabel;     // Assigned hint label (a, s, d, ...)
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
        @param finder       The IParameterFinder interface wrapper
        @param plugin       The plugin instance (for mapping ParamIDs to JUCE indices)
        @param nativeWidth  Editor width in native (unscaled) pixels
        @param nativeHeight Editor height in native (unscaled) pixels
        @param gridStep     Scan interval in pixels (default 8)
    */
    void scan (VST3ParameterFinderSupport& finder,
               juce::AudioPluginInstance* plugin,
               int nativeWidth, int nativeHeight,
               int gridStep = 8);

    bool hasResults() const { return ! results.empty(); }
    const std::vector<SpatialParamInfo>& getResults() const { return results; }
    void clear() { results.clear(); }

private:
    std::vector<SpatialParamInfo> results;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterFinderScanner)
};

} // namespace dc
