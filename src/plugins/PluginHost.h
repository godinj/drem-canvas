#pragma once
#include <JuceHeader.h>  // TODO: Phase 4 Agent 06 — remove once all callers migrated
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

    // --- New dc:: API ---

    using DcPluginCallback = std::function<void (std::unique_ptr<dc::PluginInstance>, const std::string& error)>;
    void createPluginAsyncDc (const dc::PluginDescription& desc,
                               double sampleRate, int blockSize,
                               DcPluginCallback callback);

    std::unique_ptr<dc::PluginInstance> createPluginSyncDc (
        const dc::PluginDescription& desc,
        double sampleRate, int blockSize);

    static std::string savePluginStateDc (dc::PluginInstance& plugin);
    static void restorePluginStateDc (dc::PluginInstance& plugin, const std::string& base64State);

    static dc::PluginDescription descriptionFromPropertyTreeDc (const PropertyTree& pluginNode);

    // --- Backward-compatible JUCE API (TODO: Phase 4 Agent 06 — remove) ---

    using PluginCallback = std::function<void (std::unique_ptr<juce::AudioPluginInstance>, const std::string& error)>;
    void createPluginAsync (const juce::PluginDescription& desc,
                            double sampleRate, int blockSize,
                            PluginCallback callback);

    static std::unique_ptr<juce::AudioPluginInstance> createPluginSync (
        juce::AudioPluginFormatManager& formatManager,
        const juce::PluginDescription& desc,
        double sampleRate, int blockSize);

    static std::string savePluginState (juce::AudioPluginInstance& plugin);
    static void restorePluginState (juce::AudioPluginInstance& plugin, const std::string& base64State);

    static juce::PluginDescription descriptionFromPropertyTree (const PropertyTree& pluginNode);

private:
    PluginManager& pluginManager;

    PluginHost (const PluginHost&) = delete;
    PluginHost& operator= (const PluginHost&) = delete;
};

} // namespace dc
