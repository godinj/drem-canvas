#include "dc/plugins/PluginScanner.h"
#include "dc/plugins/VST3Module.h"
#include "dc/foundation/assert.h"

#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace dc {

namespace
{
    /// Expand ~ to $HOME at the start of a path
    std::filesystem::path expandHome(const std::string& pathStr)
    {
        if (pathStr.empty() || pathStr[0] != '~')
            return pathStr;

        const char* home = std::getenv("HOME");

        if (home == nullptr)
            return pathStr;

        return std::filesystem::path(home) / pathStr.substr(2);
    }

    /// Case-insensitive check for .vst3 extension
    bool hasVst3Extension(const std::filesystem::path& p)
    {
        auto ext = p.extension().string();

        if (ext.size() != 5)  // ".vst3" is 5 characters including the dot
            return false;

        // Convert to lowercase for comparison
        for (auto& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        return ext == ".vst3";
    }

    /// Serialize a PluginDescription to a string of key=value lines
    /// terminated by a blank line.
    std::string serializeDescription(const PluginDescription& desc)
    {
        std::ostringstream ss;
        auto m = desc.toMap();

        for (const auto& kv : m)
            ss << kv.first << "=" << kv.second << "\n";

        ss << "\n";  // blank line terminates record
        return ss.str();
    }

    /// Deserialize a PluginDescription from key=value lines.
    /// Returns nullopt if the data is empty or malformed.
    std::optional<PluginDescription> deserializeDescription(const std::string& data)
    {
        if (data.empty())
            return std::nullopt;

        std::map<std::string, std::string> m;
        std::istringstream ss(data);
        std::string line;

        while (std::getline(ss, line))
        {
            if (line.empty())
                break;  // blank line terminates record

            auto eq = line.find('=');

            if (eq == std::string::npos)
                continue;

            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);
            m[key] = val;
        }

        if (m.empty())
            return std::nullopt;

        return PluginDescription::fromMap(m);
    }

    /// Convert a char16 (UTF-16) name to std::string (ASCII subset).
    /// VST3 SDK uses Steinberg::char16 (typedef for char16_t).
    std::string char16ToString(const Steinberg::char16* str, int maxLen)
    {
        std::string result;
        result.reserve(static_cast<size_t>(maxLen));

        for (int i = 0; i < maxLen && str[i] != 0; ++i)
        {
            auto ch = str[i];

            if (ch < 128)
                result += static_cast<char>(ch);
            else
                result += '?';  // non-ASCII fallback
        }

        return result;
    }
} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────

PluginScanner::PluginScanner()
{
    // Set up dead man's pedal path in user's temp/cache directory
    const char* tmpDir = std::getenv("TMPDIR");

    if (tmpDir == nullptr)
        tmpDir = "/tmp";

    deadMansPedal_ = std::filesystem::path(tmpDir) / "drem-canvas-scan-pedal.txt";
}

// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::filesystem::path> PluginScanner::getDefaultSearchPaths()
{
    std::vector<std::filesystem::path> paths;

#if defined(__APPLE__)
    auto userPath = expandHome("~/Library/Audio/Plug-Ins/VST3/");
    auto systemPath = std::filesystem::path("/Library/Audio/Plug-Ins/VST3/");
#elif defined(__linux__)
    auto userPath = expandHome("~/.vst3/");
    auto systemPath = std::filesystem::path("/usr/lib/vst3/");
    auto localPath = std::filesystem::path("/usr/local/lib/vst3/");
#else
    #error "Unsupported platform"
#endif

    if (std::filesystem::is_directory(userPath))
        paths.push_back(userPath);

    if (std::filesystem::is_directory(systemPath))
        paths.push_back(systemPath);

#if defined(__linux__)
    if (std::filesystem::is_directory(localPath))
        paths.push_back(localPath);
#endif

    return paths;
}

// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::filesystem::path> PluginScanner::findBundles(
    const std::filesystem::path& searchDir)
{
    std::vector<std::filesystem::path> bundles;
    std::error_code ec;

    auto options = std::filesystem::directory_options::follow_directory_symlink
                 | std::filesystem::directory_options::skip_permission_denied;

    for (auto it = std::filesystem::recursive_directory_iterator(searchDir, options, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
        {
            dc_log("PluginScanner: error iterating: %s", ec.message().c_str());
            ec.clear();
            continue;
        }

        if (it->is_directory() && hasVst3Extension(it->path()))
        {
            bundles.push_back(it->path());
            it.disable_recursion_pending();  // don't descend into the .vst3 bundle
        }
    }

    std::sort(bundles.begin(), bundles.end());
    return bundles;
}

// ─────────────────────────────────────────────────────────────────────────────

std::optional<PluginDescription> PluginScanner::scanOne(
    const std::filesystem::path& bundlePath)
{
    // 1. Load the module
    auto module = VST3Module::load(bundlePath);

    if (! module)
        return std::nullopt;

    // 2. Get the factory
    auto* factory = module->getFactory();

    if (factory == nullptr)
        return std::nullopt;

    // 3. Get factory info for vendor/manufacturer fallback
    Steinberg::PFactoryInfo factoryInfo;
    factory->getFactoryInfo(&factoryInfo);

    // 4. Try to get IPluginFactory2 for richer class info
    Steinberg::IPluginFactory2* factory2 = nullptr;
    factory->queryInterface(Steinberg::IPluginFactory2::iid,
                            reinterpret_cast<void**>(&factory2));

    // 5. Iterate classes and find audio effects
    auto numClasses = factory->countClasses();

    for (Steinberg::int32 i = 0; i < numClasses; ++i)
    {
        Steinberg::PClassInfo classInfo;

        if (factory->getClassInfo(i, &classInfo) != Steinberg::kResultOk)
            continue;

        // Only interested in audio effect classes
        if (std::strcmp(classInfo.category, kVstAudioEffectClass) != 0)
            continue;

        PluginDescription desc;
        desc.name = classInfo.name;
        desc.uid = PluginDescription::uidToHexString(classInfo.cid);
        desc.path = bundlePath;
        desc.category = classInfo.category;
        desc.manufacturer = factoryInfo.vendor;

        // Try to get richer info from IPluginFactory2
        if (factory2 != nullptr)
        {
            Steinberg::PClassInfo2 classInfo2;

            if (factory2->getClassInfo2(i, &classInfo2) == Steinberg::kResultOk)
            {
                desc.name = classInfo2.name;

                if (std::strlen(classInfo2.vendor) > 0)
                    desc.manufacturer = classInfo2.vendor;

                desc.version = classInfo2.version;
                desc.category = classInfo2.subCategories;
            }
        }

        // 6. Try to create the component to query bus info
        void* obj = nullptr;

        if (factory->createInstance(classInfo.cid,
                                    Steinberg::Vst::IComponent::iid,
                                    &obj) == Steinberg::kResultOk
            && obj != nullptr)
        {
            auto* component = static_cast<Steinberg::Vst::IComponent*>(obj);

            // Initialize the component (required before querying bus info)
            component->initialize(nullptr);

            // Query audio input channels
            auto audioInputBusCount = component->getBusCount(
                Steinberg::Vst::kAudio, Steinberg::Vst::kInput);

            if (audioInputBusCount > 0)
            {
                Steinberg::Vst::BusInfo busInfo;

                if (component->getBusInfo(Steinberg::Vst::kAudio,
                                          Steinberg::Vst::kInput,
                                          0, busInfo) == Steinberg::kResultOk)
                {
                    desc.numInputChannels = busInfo.channelCount;
                }
            }

            // Query audio output channels
            auto audioOutputBusCount = component->getBusCount(
                Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);

            if (audioOutputBusCount > 0)
            {
                Steinberg::Vst::BusInfo busInfo;

                if (component->getBusInfo(Steinberg::Vst::kAudio,
                                          Steinberg::Vst::kOutput,
                                          0, busInfo) == Steinberg::kResultOk)
                {
                    desc.numOutputChannels = busInfo.channelCount;
                }
            }

            // Check for MIDI support (event input bus)
            auto eventInputBusCount = component->getBusCount(
                Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
            desc.acceptsMidi = (eventInputBusCount > 0);

            // Check for MIDI output (event output bus)
            auto eventOutputBusCount = component->getBusCount(
                Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);
            desc.producesMidi = (eventOutputBusCount > 0);

            // Check for editor: try to get controller class ID
            Steinberg::TUID controllerCid;

            if (component->getControllerClassId(controllerCid) == Steinberg::kResultOk)
            {
                // Controller exists — assume it has an editor.
                // A more thorough check would create the controller and query
                // IPlugView, but that's expensive for scanning.
                desc.hasEditor = true;
            }
            else
            {
                // Some plugins implement IEditController on the same component
                Steinberg::Vst::IEditController* controller = nullptr;

                if (component->queryInterface(
                        Steinberg::Vst::IEditController::iid,
                        reinterpret_cast<void**>(&controller))
                    == Steinberg::kResultOk
                    && controller != nullptr)
                {
                    desc.hasEditor = true;
                    controller->release();
                }
            }

            // Clean up
            component->terminate();
            component->release();
        }

        // Release factory2 if we obtained it
        if (factory2 != nullptr)
            factory2->release();

        // Return the first valid audio effect found
        return desc;
    }

    // No audio effect class found
    if (factory2 != nullptr)
        factory2->release();

    return std::nullopt;
}

// ─────────────────────────────────────────────────────────────────────────────

std::optional<PluginDescription> PluginScanner::scanOneForked(
    const std::filesystem::path& bundlePath)
{
    // 1. Write bundle path to dead man's pedal (for crash recovery)
    {
        std::ofstream pedal(deadMansPedal_);
        pedal << bundlePath.string() << std::endl;
    }

    // 2. Create pipe for child→parent communication
    int pipefd[2];

    if (pipe(pipefd) != 0)
    {
        dc_log("PluginScanner: pipe() failed");
        return std::nullopt;
    }

    // 3. Fork
    pid_t pid = fork();

    if (pid < 0)
    {
        dc_log("PluginScanner: fork() failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return std::nullopt;
    }

    if (pid == 0)
    {
        // ── Child process ──────────────────────────────────────────────
        close(pipefd[0]);  // close read end

        auto result = scanOne(bundlePath);

        if (result.has_value())
        {
            auto data = serializeDescription(result.value());
            auto bytesWritten = write(pipefd[1], data.c_str(), data.size());
            (void) bytesWritten;  // ignore write errors in child
        }

        close(pipefd[1]);
        _exit(result.has_value() ? 0 : 1);
    }

    // ── Parent process ─────────────────────────────────────────────────
    close(pipefd[1]);  // close write end

    // Set up timeout: 10 seconds
    static constexpr int kTimeoutSeconds = 10;

    // Use a polling approach for the timeout since alarm() is process-global
    // and could interfere with other parts of the application.
    int status = 0;
    bool childFinished = false;

    for (int elapsed = 0; elapsed < kTimeoutSeconds * 10; ++elapsed)
    {
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == pid)
        {
            childFinished = true;
            break;
        }
        else if (result < 0)
        {
            // waitpid error
            break;
        }

        // Sleep 100ms
        usleep(100000);
    }

    if (! childFinished)
    {
        // Timeout — kill the child
        dc_log("PluginScanner: child timed out scanning: %s",
               bundlePath.string().c_str());
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);  // reap zombie
        close(pipefd[0]);
        std::filesystem::remove(deadMansPedal_);
        return std::nullopt;
    }

    // Check if child exited normally with status 0
    if (! WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        if (WIFSIGNALED(status))
        {
            dc_log("PluginScanner: child crashed (signal %d) scanning: %s",
                   WTERMSIG(status), bundlePath.string().c_str());
        }
        else
        {
            dc_log("PluginScanner: child failed scanning: %s",
                   bundlePath.string().c_str());
        }

        close(pipefd[0]);
        std::filesystem::remove(deadMansPedal_);
        return std::nullopt;
    }

    // Read serialized data from pipe
    std::string data;
    char buf[4096];
    ssize_t bytesRead;

    while ((bytesRead = read(pipefd[0], buf, sizeof(buf))) > 0)
        data.append(buf, static_cast<size_t>(bytesRead));

    close(pipefd[0]);

    // 4. Clean up dead man's pedal
    std::error_code ec;
    std::filesystem::remove(deadMansPedal_, ec);

    if (data.empty())
        return std::nullopt;

    return deserializeDescription(data);
}

// ─────────────────────────────────────────────────────────────────────────────

std::vector<PluginDescription> PluginScanner::scanAll()
{
    // 1. Collect all .vst3 bundles from search paths
    std::vector<std::filesystem::path> allBundles;
    auto searchPaths = getDefaultSearchPaths();

    for (const auto& dir : searchPaths)
    {
        auto bundles = findBundles(dir);
        allBundles.insert(allBundles.end(), bundles.begin(), bundles.end());
    }

    dc_log("PluginScanner: found %d VST3 bundle(s) to scan",
           static_cast<int>(allBundles.size()));

    // 2. Scan each bundle via forked subprocess
    std::vector<PluginDescription> results;
    int total = static_cast<int>(allBundles.size());

    for (int i = 0; i < total; ++i)
    {
        const auto& bundle = allBundles[static_cast<size_t>(i)];
        auto pluginName = bundle.stem().string();

        // 3. Report progress
        if (progressCallback_)
            progressCallback_(pluginName, i + 1, total);

        auto desc = scanOneForked(bundle);

        if (desc.has_value())
        {
            results.push_back(std::move(desc.value()));
            dc_log("PluginScanner: scanned OK: %s", pluginName.c_str());
        }
        else
        {
            dc_log("PluginScanner: failed to scan: %s", pluginName.c_str());
        }
    }

    dc_log("PluginScanner: scan complete — %d plugin(s) found",
           static_cast<int>(results.size()));

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────

void PluginScanner::setProgressCallback(ProgressCallback cb)
{
    progressCallback_ = std::move(cb);
}

} // namespace dc
