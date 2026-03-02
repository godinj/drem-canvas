#include "dc/plugins/VST3Module.h"
#include "dc/foundation/assert.h"

#include <pluginterfaces/base/ipluginbase.h>

#include <dlfcn.h>
#include <string>

namespace dc {

namespace
{
    std::filesystem::path resolveLibraryPath(const std::filesystem::path& bundlePath)
    {
        auto stem = bundlePath.stem().string();

#if defined(__APPLE__)
        return bundlePath / "Contents" / "MacOS" / stem;
#elif defined(__linux__)
        return bundlePath / "Contents" / "x86_64-linux" / (stem + ".so");
#else
        #error "Unsupported platform"
#endif
    }
} // anonymous namespace

std::unique_ptr<VST3Module> VST3Module::load(const std::filesystem::path& bundlePath)
{
    if (! std::filesystem::exists(bundlePath))
    {
        dc_log(("VST3Module: bundle not found: " + bundlePath.string()).c_str());
        return nullptr;
    }

    auto libPath = resolveLibraryPath(bundlePath);

    if (! std::filesystem::exists(libPath))
    {
        dc_log(("VST3Module: library not found: " + libPath.string()).c_str());
        return nullptr;
    }

    // Open the shared library
    void* handle = dlopen(libPath.string().c_str(), RTLD_LAZY);

    if (handle == nullptr)
    {
        auto err = std::string("VST3Module: dlopen failed: ") + (dlerror() ? dlerror() : "unknown error");
        dc_log(err.c_str());
        return nullptr;
    }

    // Look up init/exit functions (platform-specific names)
#if defined(__APPLE__)
    auto* initFunc = reinterpret_cast<InitModuleFunc>(dlsym(handle, "bundleEntry"));
    auto* exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "bundleExit"));
#elif defined(__linux__)
    auto* initFunc = reinterpret_cast<InitModuleFunc>(dlsym(handle, "InitDll"));
    auto* exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "ExitDll"));
#endif

    // Call the init function (if present)
    if (initFunc != nullptr)
    {
        if (! initFunc())
        {
            dc_log("VST3Module: init function returned false");
            dlclose(handle);
            return nullptr;
        }
    }

    // Look up GetPluginFactory
    using GetFactoryFunc = Steinberg::IPluginFactory* (*)();
    auto* getFactory = reinterpret_cast<GetFactoryFunc>(dlsym(handle, "GetPluginFactory"));

    if (getFactory == nullptr)
    {
        dc_log("VST3Module: GetPluginFactory not found");
        if (exitFunc != nullptr)
            exitFunc();
        dlclose(handle);
        return nullptr;
    }

    // Get the factory
    auto* factory = getFactory();

    if (factory == nullptr)
    {
        dc_log("VST3Module: GetPluginFactory returned null");
        if (exitFunc != nullptr)
            exitFunc();
        dlclose(handle);
        return nullptr;
    }

    // Build the module object
    auto module = std::unique_ptr<VST3Module>(new VST3Module());
    module->libraryHandle_ = handle;
    module->factory_ = factory;
    module->path_ = bundlePath;
    module->exitFunc_ = exitFunc;

    return module;
}

VST3Module::~VST3Module()
{
    if (factory_ != nullptr)
    {
        factory_->release();
        factory_ = nullptr;
    }

    if (exitFunc_ != nullptr)
        exitFunc_();

    if (libraryHandle_ != nullptr)
    {
        dlclose(libraryHandle_);
        libraryHandle_ = nullptr;
    }
}

Steinberg::IPluginFactory* VST3Module::getFactory() const
{
    return factory_;
}

const std::filesystem::path& VST3Module::getPath() const
{
    return path_;
}

} // namespace dc
