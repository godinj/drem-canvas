#include "dc/plugins/VST3Host.h"
#include "dc/foundation/assert.h"
#include "dc/foundation/file_utils.h"
#include <yaml-cpp/yaml.h>
#include <thread>

namespace dc {

VST3Host::VST3Host() = default;
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
        dc_log (e.what());
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

    auto module = VST3Module::load (bundlePath);
    if (! module)
        return nullptr;

    auto* raw = module.get();
    loadedModules_.emplace (key, std::move (module));
    return raw;
}

} // namespace dc
