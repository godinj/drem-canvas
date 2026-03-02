#pragma once
#include "PluginManager.h"
#include "dc/plugins/VST3Host.h"
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginDescription.h"
#include "dc/model/PropertyTree.h"
#include <string>

namespace dc
{

class PluginHost
{
public:
    explicit PluginHost (PluginManager& manager);

    // --- Plugin creation ---

    using PluginCallback = std::function<void (std::unique_ptr<dc::PluginInstance>, const std::string& error)>;
    void createPluginAsync (const dc::PluginDescription& desc,
                            double sampleRate, int blockSize,
                            PluginCallback callback);

    std::unique_ptr<dc::PluginInstance> createPluginSync (
        const dc::PluginDescription& desc,
        double sampleRate, int blockSize);

    // --- State management ---

    static std::string savePluginState (dc::PluginInstance& plugin);
    static void restorePluginState (dc::PluginInstance& plugin, const std::string& base64State);

    // --- Description from model ---

    static dc::PluginDescription descriptionFromPropertyTree (const PropertyTree& pluginNode);

private:
    PluginManager& pluginManager;

    PluginHost (const PluginHost&) = delete;
    PluginHost& operator= (const PluginHost&) = delete;
};

} // namespace dc
