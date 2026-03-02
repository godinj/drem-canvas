#include "dc/plugins/VST3Host.h"
#include "dc/foundation/assert.h"
#include "dc/foundation/file_utils.h"
#include <yaml-cpp/yaml.h>
#include <thread>

namespace dc {

VST3Host::VST3Host()
    : probeCache_ (dc::getUserAppDataDirectory())
{
    probeCache_.load();
}

VST3Host::~VST3Host() = default;

void VST3Host::scanPlugins (PluginScanner::ProgressCallback cb)
{
    if (cb)
        scanner_.setProgressCallback (std::move (cb));

    knownPlugins_ = scanner_.scanAll();
}

const std::vector<PluginDescription>& VST3Host::getKnownPlugins() const
{
    return knownPlugins_;
}

void VST3Host::setKnownPlugins (std::vector<PluginDescription> plugins)
{
    knownPlugins_ = std::move (plugins);
}

void VST3Host::loadDatabase (const std::filesystem::path& path)
{
    try
    {
        auto content = dc::readFileToString (path);
        if (content.empty())
            return;

        YAML::Node root = YAML::Load (content);

        if (! root["plugins"] || ! root["plugins"].IsSequence())
            return;

        std::vector<PluginDescription> loaded;

        for (const auto& node : root["plugins"])
        {
            std::map<std::string, std::string> m;

            for (auto it = node.begin(); it != node.end(); ++it)
                m[it->first.as<std::string>()] = it->second.as<std::string>();

            loaded.push_back (PluginDescription::fromMap (m));
        }

        knownPlugins_ = std::move (loaded);
    }
    catch (const YAML::Exception& e)
    {
        dc_log ("%s", e.what());
    }
}

void VST3Host::saveDatabase (const std::filesystem::path& path) const
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "plugins";
    out << YAML::Value << YAML::BeginSeq;

    for (const auto& desc : knownPlugins_)
    {
        auto m = desc.toMap();
        out << YAML::BeginMap;
        for (const auto& kv : m)
            out << YAML::Key << kv.first << YAML::Value << kv.second;
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;
    out << YAML::EndMap;

    std::filesystem::create_directories (path.parent_path());
    dc::writeStringToFile (path, out.c_str());
}

std::unique_ptr<PluginInstance> VST3Host::createInstanceSync (
    const PluginDescription& desc,
    double sampleRate, int maxBlockSize)
{
    auto* module = getOrLoadModule (desc.path);
    if (module == nullptr)
    {
        dc_log ("VST3Host: failed to load module for plugin");
        return nullptr;
    }

    auto instance = PluginInstance::create (*module, desc, sampleRate, maxBlockSize);
    if (! instance)
    {
        dc_log ("VST3Host: failed to create plugin instance");
    }

    return instance;
}

void VST3Host::createInstance (const PluginDescription& desc,
                                double sampleRate, int maxBlockSize,
                                CreateCallback callback)
{
    // Run synchronous creation on a detached background thread.
    // The callback is invoked directly from the background thread.
    // Callers that need message-thread delivery should post through
    // dc::MessageQueue themselves.
    std::thread ([this, desc, sampleRate, maxBlockSize,
                  cb = std::move (callback)] ()
    {
        auto instance = createInstanceSync (desc, sampleRate, maxBlockSize);

        std::string error;
        if (! instance)
            error = "Failed to create plugin: " + desc.name;

        cb (std::move (instance), std::move (error));
    }).detach();
}

const PluginDescription* VST3Host::findByUid (const std::string& uid) const
{
    for (const auto& desc : knownPlugins_)
    {
        if (desc.uid == uid)
            return &desc;
    }
    return nullptr;
}

VST3Module* VST3Host::getOrLoadModule (const std::filesystem::path& bundlePath)
{
    auto key = bundlePath.string();

    std::lock_guard<std::mutex> lock (moduleMutex_);

    auto it = loadedModules_.find (key);
    if (it != loadedModules_.end())
        return it->second.get();

    auto status = probeCache_.getStatus (bundlePath);

    // Previously blocked and bundle hasn't changed — skip
    if (status == ProbeCache::Status::blocked)
    {
        dc_log ("VST3Host: skipping blocked module: %s", key.c_str());
        return nullptr;
    }

    // Previously safe — skip probe, load directly with pedal protection.
    // If the load crashes (plugin updated and now broken), the pedal
    // file survives and next startup will mark it as blocked.
    if (status == ProbeCache::Status::safe)
    {
        probeCache_.setPedal (bundlePath);
        auto module = VST3Module::load (bundlePath, /* skipProbe */ true);
        probeCache_.clearPedal();

        if (module)
        {
            auto* raw = module.get();
            loadedModules_.emplace (key, std::move (module));
            return raw;
        }

        // Cached as safe but load failed — invalidate
        probeCache_.setStatus (bundlePath, ProbeCache::Status::unknown);
        probeCache_.save();
        return nullptr;
    }

    // Yabridge chainloaders can't be fork-probed: the forked child
    // can't set up the Wine bridge, so GetPluginFactory() always aborts.
    // Load them directly in-process with pedal protection.
    if (VST3Module::isYabridgeBundle (bundlePath))
    {
        dc_log ("VST3Host: yabridge detected, loading directly: %s", key.c_str());
        probeCache_.setPedal (bundlePath);
        auto module = VST3Module::load (bundlePath, /* skipProbe */ true);
        probeCache_.clearPedal();

        if (module)
        {
            probeCache_.setStatus (bundlePath, ProbeCache::Status::safe);
            probeCache_.save();

            auto* raw = module.get();
            loadedModules_.emplace (key, std::move (module));
            return raw;
        }

        dc_log ("VST3Host: yabridge load failed: %s", key.c_str());
        probeCache_.setStatus (bundlePath, ProbeCache::Status::blocked);
        probeCache_.save();
        return nullptr;
    }

    // Native plugin — run the fork probe first
    if (VST3Module::probeModuleSafe (bundlePath))
    {
        probeCache_.setPedal (bundlePath);
        auto module = VST3Module::load (bundlePath, /* skipProbe */ true);
        probeCache_.clearPedal();

        if (module)
        {
            probeCache_.setStatus (bundlePath, ProbeCache::Status::safe);
            probeCache_.save();

            auto* raw = module.get();
            loadedModules_.emplace (key, std::move (module));
            return raw;
        }
        return nullptr;
    }

    // Native probe failed — cache as blocked
    dc_log ("VST3Host: probe failed, marking blocked: %s", key.c_str());
    probeCache_.setStatus (bundlePath, ProbeCache::Status::blocked);
    probeCache_.save();
    return nullptr;
}

} // namespace dc
