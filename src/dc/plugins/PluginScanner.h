#pragma once
#include "PluginDescription.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dc {

class PluginScanner
{
public:
    PluginScanner();

    /// Scan all standard VST3 directories. Returns all discovered plugins.
    /// Each plugin is scanned in a forked subprocess for crash isolation.
    std::vector<PluginDescription> scanAll();

    /// Scan a single .vst3 bundle path (in-process, no fork).
    /// Returns nullopt if the bundle is invalid or scanning fails.
    std::optional<PluginDescription> scanOne(
        const std::filesystem::path& bundlePath);

    /// Get standard VST3 search paths for the current platform.
    static std::vector<std::filesystem::path> getDefaultSearchPaths();

    /// Progress callback: (pluginName, current, total)
    using ProgressCallback = std::function<void(const std::string& pluginName,
                                                 int current, int total)>;
    void setProgressCallback(ProgressCallback cb);

private:
    ProgressCallback progressCallback_;
    std::filesystem::path deadMansPedal_;  // tracks current scan target

    /// Scan a single bundle in a forked child process.
    /// Returns nullopt if child crashes or times out.
    std::optional<PluginDescription> scanOneForked(
        const std::filesystem::path& bundlePath);

    /// Enumerate .vst3 bundles in a directory (non-recursive into bundles).
    static std::vector<std::filesystem::path> findBundles(
        const std::filesystem::path& searchDir);
};

} // namespace dc
