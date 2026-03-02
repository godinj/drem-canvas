#include "PluginManager.h"
#include "dc/foundation/file_utils.h"

namespace dc
{

PluginManager::PluginManager (MessageQueue& mq)
    : messageQueue_ (mq)
{
}

PluginManager::~PluginManager()
{
    scanThread_.stop();
}

void PluginManager::scanForPlugins()
{
    vst3Host_.scanPlugins();
    savePluginList (getDefaultPluginListFile());
}

const std::vector<dc::PluginDescription>& PluginManager::getKnownPlugins() const
{
    return vst3Host_.getKnownPlugins();
}

dc::VST3Host& PluginManager::getVST3Host()
{
    return vst3Host_;
}

void PluginManager::scanForPluginsAsync (ScanProgressCallback onProgress,
                                          ScanCompleteCallback onComplete)
{
    if (scanning_.load())
        return;

    scanning_.store (true);

    scanThread_.submit ([this, onProgress = std::move (onProgress),
                         onComplete = std::move (onComplete)]()
    {
        vst3Host_.scanPlugins ([this, &onProgress] (const std::string& name,
                                                     int current, int total)
        {
            if (onProgress)
            {
                messageQueue_.post ([onProgress, name, current, total]()
                {
                    onProgress (name, current, total);
                });
            }
        });

        savePluginList (getDefaultPluginListFile());

        messageQueue_.post ([this, onComplete]()
        {
            scanning_.store (false);
            if (onComplete)
                onComplete();
        });
    });
}

bool PluginManager::isScanning() const
{
    return scanning_.load();
}

void PluginManager::savePluginList (const std::filesystem::path& file) const
{
    vst3Host_.saveDatabase (file);
}

void PluginManager::loadPluginList (const std::filesystem::path& file)
{
    vst3Host_.loadDatabase (file);
}

std::filesystem::path PluginManager::getDefaultPluginListFile() const
{
    return dc::getUserAppDataDirectory() / "pluginList.yaml";
}

} // namespace dc
