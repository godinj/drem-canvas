#pragma once
#include "PluginDescription.h"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dc {

class ProbeCache;
class VST3Module;

class PluginScanner
{
public:
    PluginScanner();

    /// Scan all standard VST3 directories. Returns all discovered plugins.
    /// Each plugin is scanned in a forked subprocess for crash isolation.
    /// Yabridge bundles are scanned in-process (fork probe cannot initialise
    /// the Wine bridge).
    std::vector<PluginDescription> scanAll();

    /// Scan a single .vst3 bundle path (in-process, no fork).
    /// Returns nullopt if the bundle is invalid or scanning fails.
    std::optional<PluginDescription> scanOne (
        const std::filesystem::path& bundlePath);

    /// Get standard VST3 search paths for the current platform.
    static std::vector<std::filesystem::path> getDefaultSearchPaths();

    /// Recursively enumerate .vst3 bundles under a directory.
    /// Does not descend into .vst3 bundle directories themselves.
    static std::vector<std::filesystem::path> findBundles (
        const std::filesystem::path& searchDir);

    /// Progress callback: (pluginName, current, total)
    using ProgressCallback = std::function<void (const std::string& pluginName,
                                                  int current, int total)>;
    void setProgressCallback (ProgressCallback cb);

    /// Set the probe cache for yabridge in-process scanning.
    /// Must be called before scanAll() if yabridge plugins are present.
    void setProbeCache (ProbeCache* cache);

    /// Set the previous known plugin list for incremental scan.
    /// Plugins whose bundle mtime hasn't changed will be reused from this list
    /// instead of being rescanned.
    void setPreviousPlugins (const std::vector<PluginDescription>& plugins);

    /// Scan a yabridge bundle in-process with pedal protection.
    /// Returns nullopt if the load fails or the module has no audio effect class.
    std::optional<PluginDescription> scanOneInProcess (
        const std::filesystem::path& bundlePath);

private:
    ProgressCallback progressCallback_;
    std::filesystem::path deadMansPedal_;  // tracks current scan target
    ProbeCache* probeCache_ = nullptr;
    std::vector<PluginDescription> previousPlugins_;

    /// Scan a single bundle in a forked child process.
    /// Returns nullopt if child crashes or times out.
    std::optional<PluginDescription> scanOneForked (
        const std::filesystem::path& bundlePath);

    /// Extract PluginDescription from an already-loaded module.
    static std::optional<PluginDescription> extractDescription (
        VST3Module& module, const std::filesystem::path& bundlePath);
};

} // namespace dc
