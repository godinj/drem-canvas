#pragma once

#include "ParameterFinderScanner.h"
#include <filesystem>
#include <string>
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
    static void save (const std::string& pluginFileOrIdentifier,
                      const std::string& pluginName,
                      int editorWidth, int editorHeight,
                      const std::vector<SpatialParamInfo>& results);

    /** Load cached scan results from disk.
        Returns true if a valid cache file was found and results were populated.
        The caller must regenerate hintLabels after loading. */
    static bool load (const std::string& pluginFileOrIdentifier,
                      int editorWidth, int editorHeight,
                      std::vector<SpatialParamInfo>& results);

    /** Delete the cache file for a given plugin + editor size. */
    static void invalidate (const std::string& pluginFileOrIdentifier,
                            int editorWidth, int editorHeight);

private:
    static std::filesystem::path getCacheDir();
    static std::filesystem::path getCacheFile (const std::string& pluginFileOrIdentifier,
                                               int editorWidth, int editorHeight);

    SpatialScanCache() = delete;
    SpatialScanCache (const SpatialScanCache&) = delete;
    SpatialScanCache& operator= (const SpatialScanCache&) = delete;
};

} // namespace dc
