#pragma once
#include "dc/plugins/PluginDescription.h"
#include "dc/plugins/PluginScanner.h"
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/ProbeCache.h"
#include "dc/plugins/VST3Module.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

class VST3Host
{
public:
    VST3Host();
    ~VST3Host();

    /// Scan for plugins (out-of-process, crash-safe)
    void scanPlugins (PluginScanner::ProgressCallback cb = {});

    /// Get the known plugin database
    const std::vector<PluginDescription>& getKnownPlugins() const;

    /// Set the known plugin list (e.g., after loading from YAML)
    void setKnownPlugins (std::vector<PluginDescription> plugins);

    /// Load plugin database from a YAML file
    void loadDatabase (const std::filesystem::path& path);

    /// Save plugin database to a YAML file
    void saveDatabase (const std::filesystem::path& path) const;

    /// Create a plugin instance (sync, blocking)
    std::unique_ptr<PluginInstance> createInstanceSync (
        const PluginDescription& desc,
        double sampleRate, int maxBlockSize);

    /// Create a plugin instance (async, callback on completion)
    using CreateCallback = std::function<void (
        std::unique_ptr<PluginInstance> instance,
        std::string error)>;
    void createInstance (const PluginDescription& desc,
                         double sampleRate, int maxBlockSize,
                         CreateCallback callback);

    /// Find a plugin description by UID
    const PluginDescription* findByUid (const std::string& uid) const;

private:
    PluginScanner scanner_;
    ProbeCache probeCache_;
    std::vector<PluginDescription> knownPlugins_;

    /// Cache of loaded modules (keyed by bundle path string)
    std::unordered_map<std::string, std::unique_ptr<VST3Module>> loadedModules_;

    /// Mutex for module cache (async creates may access from different threads)
    std::mutex moduleMutex_;

    /// Get or load a module for a bundle path
    VST3Module* getOrLoadModule (const std::filesystem::path& bundlePath);

    VST3Host (const VST3Host&) = delete;
    VST3Host& operator= (const VST3Host&) = delete;
};

} // namespace dc
