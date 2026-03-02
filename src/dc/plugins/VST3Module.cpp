#include "dc/plugins/VST3Module.h"
#include "dc/foundation/assert.h"

#include <pluginterfaces/base/ipluginbase.h>

#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

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

bool VST3Module::probeModuleSafe(const std::filesystem::path& bundlePath)
{
    auto libPath = resolveLibraryPath(bundlePath);

    if (! std::filesystem::exists(libPath))
        return false;

    pid_t pid = fork();

    if (pid < 0)
    {
        dc_log("VST3Module: fork() failed for probe");
        return true;  // optimistic: allow load attempt
    }

    if (pid == 0)
    {
        // ── Child process: attempt to load the module ──
        void* handle = dlopen(libPath.string().c_str(), RTLD_LAZY);
        if (handle == nullptr)
            _exit(1);

#if defined(__APPLE__)
        auto* initFunc = reinterpret_cast<InitModuleFunc>(dlsym(handle, "bundleEntry"));
        auto* exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "bundleExit"));
#elif defined(__linux__)
        // Try modern VST3 SDK names first (ModuleEntry takes void*),
        // then fall back to legacy names (InitDll takes no args).
        auto* moduleEntry = reinterpret_cast<ModuleEntryFunc>(dlsym(handle, "ModuleEntry"));
        auto* exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "ModuleExit"));
        InitModuleFunc initFunc = nullptr;
        if (moduleEntry == nullptr)
        {
            initFunc = reinterpret_cast<InitModuleFunc>(dlsym(handle, "InitDll"));
            exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "ExitDll"));
        }
#endif

        // Call the appropriate init function
#if defined(__linux__)
        bool initOk = true;
        if (moduleEntry != nullptr)
            initOk = moduleEntry(handle);
        else if (initFunc != nullptr)
            initOk = initFunc();

        if (! initOk)
#else
        if (initFunc != nullptr && ! initFunc())
#endif
        {
            dlclose(handle);
            _exit(1);
        }

        using GetFactoryFunc = Steinberg::IPluginFactory* (*)();
        auto* getFactory = reinterpret_cast<GetFactoryFunc>(dlsym(handle, "GetPluginFactory"));

        if (getFactory == nullptr)
        {
            if (exitFunc != nullptr) exitFunc();
            dlclose(handle);
            _exit(1);
        }

        auto* factory = getFactory();  // may abort() — that's the whole point

        if (factory == nullptr)
        {
            if (exitFunc != nullptr) exitFunc();
            dlclose(handle);
            _exit(1);
        }

        factory->release();
        if (exitFunc != nullptr) exitFunc();
        dlclose(handle);
        _exit(0);
    }

    // ── Parent process: wait for child with timeout ──
    static constexpr int kTimeoutMs = 5000;

    for (int elapsed = 0; elapsed < kTimeoutMs; elapsed += 100)
    {
        int status = 0;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == pid)
        {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                return true;

            if (WIFSIGNALED(status))
            {
                dc_log("VST3Module: probe crashed (signal %d): %s",
                       WTERMSIG(status), bundlePath.string().c_str());
            }
            else
            {
                dc_log("VST3Module: probe failed: %s",
                       bundlePath.string().c_str());
            }

            return false;
        }

        usleep(100000);
    }

    dc_log("VST3Module: probe timed out: %s", bundlePath.string().c_str());
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    return false;
}

std::unique_ptr<VST3Module> VST3Module::load(const std::filesystem::path& bundlePath,
                                              bool skipProbe)
{
    if (! std::filesystem::exists(bundlePath))
    {
        dc_log("VST3Module: bundle not found: %s", bundlePath.string().c_str());
        return nullptr;
    }

    auto libPath = resolveLibraryPath(bundlePath);

    if (! std::filesystem::exists(libPath))
    {
        dc_log("VST3Module: library not found: %s", libPath.string().c_str());
        return nullptr;
    }

    // Probe in a forked subprocess first to survive plugins that abort()
    // (e.g. yabridge chainloaders when Wine bridge is unavailable).
    if (! skipProbe && ! probeModuleSafe(bundlePath))
    {
        dc_log("VST3Module: skipping unsafe module: %s", bundlePath.string().c_str());
        return nullptr;
    }

    // Open the shared library
    void* handle = dlopen(libPath.string().c_str(), RTLD_LAZY);

    if (handle == nullptr)
    {
        dc_log("VST3Module: dlopen failed: %s", dlerror() ? dlerror() : "unknown error");
        return nullptr;
    }

    // Look up init/exit functions (platform-specific names)
#if defined(__APPLE__)
    auto* initFunc = reinterpret_cast<InitModuleFunc>(dlsym(handle, "bundleEntry"));
    auto* exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "bundleExit"));
    ModuleEntryFunc moduleEntry = nullptr;
#elif defined(__linux__)
    // Try modern VST3 SDK names first (ModuleEntry takes void*),
    // then fall back to legacy names (InitDll takes no args).
    auto* moduleEntry = reinterpret_cast<ModuleEntryFunc>(dlsym(handle, "ModuleEntry"));
    auto* exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "ModuleExit"));
    InitModuleFunc initFunc = nullptr;
    if (moduleEntry == nullptr)
    {
        initFunc = reinterpret_cast<InitModuleFunc>(dlsym(handle, "InitDll"));
        exitFunc = reinterpret_cast<ExitModuleFunc>(dlsym(handle, "ExitDll"));
    }
#endif

    // Call the appropriate init function
    {
        bool initOk = true;
        if (moduleEntry != nullptr)
            initOk = moduleEntry(handle);
        else if (initFunc != nullptr)
            initOk = initFunc();

        if (! initOk)
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

bool VST3Module::isYabridgeBundle(const std::filesystem::path& bundlePath)
{
    // Yabridge bundles contain both a Linux chainloader and the original
    // Windows VST3 binary.  Native Linux VST3 plugins only have x86_64-linux.
    return std::filesystem::is_directory(bundlePath / "Contents" / "x86_64-win");
}

} // namespace dc
