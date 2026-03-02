#pragma once

#include "ParameterFinderScanner.h"
#include <JuceHeader.h>
#include <vector>

namespace dc
{

/**
 * Caches spatial scan results to disk as YAML files so that previously-scanned
 * plugins load instantly on subsequent opens.
 *
 * Cache directory: ~/.config/DremCanvas/spatial-cache/  (userApplicationDataDirectory)
 * Filename: hash64(pluginFileOrIdentifier:WxH).yaml
 */
class SpatialScanCache
{
public:
    /** Save scan results to disk. Writes atomically via .tmp + rename. */
    static void save (const juce::String& pluginFileOrIdentifier,
                      const juce::String& pluginName,
                      int editorWidth, int editorHeight,
                      const std::vector<SpatialParamInfo>& results);

    /** Load cached scan results from disk.
        Returns true if a valid cache file was found and results were populated.
        The caller must regenerate hintLabels after loading. */
    static bool load (const juce::String& pluginFileOrIdentifier,
                      int editorWidth, int editorHeight,
                      std::vector<SpatialParamInfo>& results);

    /** Delete the cache file for a given plugin + editor size. */
    static void invalidate (const juce::String& pluginFileOrIdentifier,
                            int editorWidth, int editorHeight);

private:
    static juce::File getCacheDir();
    static juce::File getCacheFile (const juce::String& pluginFileOrIdentifier,
                                    int editorWidth, int editorHeight);

    JUCE_DECLARE_NON_COPYABLE (SpatialScanCache)
};

} // namespace dc
