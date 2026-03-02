#pragma once
#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"
#include <map>
#include <memory>

namespace dc
{

class PluginWindowManager
{
public:
    PluginWindowManager();
    ~PluginWindowManager();

    /// Show an editor for a plugin (creates dc::PluginEditor, tracks lifetime)
    void showEditorForPlugin (dc::PluginInstance& plugin);

    /// Close the editor for a plugin
    void closeEditorForPlugin (dc::PluginInstance* plugin);

    /// Close all editors
    void closeAll();

private:
    std::map<dc::PluginInstance*, std::unique_ptr<dc::PluginEditor>> editors_;

    PluginWindowManager (const PluginWindowManager&) = delete;
    PluginWindowManager& operator= (const PluginWindowManager&) = delete;
};

} // namespace dc
