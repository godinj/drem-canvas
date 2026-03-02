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
    static std::unique_ptr<VST3Module> load(
        const std::filesystem::path& bundlePath);

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

    using InitModuleFunc = bool (*)();
    using ExitModuleFunc = bool (*)();

    ExitModuleFunc exitFunc_ = nullptr;
};

} // namespace dc
