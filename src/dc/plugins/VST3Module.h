#pragma once
#include <filesystem>
#include <memory>

namespace Steinberg { class IPluginFactory; }

namespace dc {

class VST3Module
{
public:
    /// Load a VST3 module from a bundle path.
    /// Returns nullptr on failure (logs the error).
    /// Probes the module in a forked subprocess first to survive
    /// plugins that abort() (e.g. yabridge when Wine is unavailable).
    /// Set skipProbe=true to bypass the probe (e.g. for cached-safe modules).
    static std::unique_ptr<VST3Module> load(
        const std::filesystem::path& bundlePath,
        bool skipProbe = false);

    /// Probe whether a module can be loaded without crashing.
    /// Forks a child process that attempts dlopen + GetPluginFactory.
    /// Returns true if the child exits cleanly, false on crash/timeout.
    static bool probeModuleSafe(const std::filesystem::path& bundlePath);

    /// Detect whether a bundle is a yabridge chainloader.
    /// Yabridge bundles contain both x86_64-linux/ and x86_64-win/ dirs.
    static bool isYabridgeBundle(const std::filesystem::path& bundlePath);

    ~VST3Module();

    VST3Module(const VST3Module&) = delete;
    VST3Module& operator=(const VST3Module&) = delete;

    /// Get the plugin factory (never null after successful load)
    Steinberg::IPluginFactory* getFactory() const;

    /// Get the bundle path
    const std::filesystem::path& getPath() const;

private:
    VST3Module() = default;

    void* libraryHandle_ = nullptr;  // dlopen handle
    Steinberg::IPluginFactory* factory_ = nullptr;
    std::filesystem::path path_;

    using InitModuleFunc = bool (*)();          // InitDll (legacy, no args)
    using ModuleEntryFunc = bool (*)(void*);    // ModuleEntry (modern, takes dlopen handle)
    using ExitModuleFunc = bool (*)();

    ExitModuleFunc exitFunc_ = nullptr;
};

} // namespace dc
