#include "PluginWindowManager.h"
#include "dc/foundation/assert.h"

namespace dc
{

PluginWindowManager::PluginWindowManager() = default;

PluginWindowManager::~PluginWindowManager()
{
    closeAll();
}

void PluginWindowManager::showEditorForPlugin (dc::PluginInstance& plugin)
{
    auto it = editors_.find (&plugin);

    if (it != editors_.end())
    {
        // Editor already open — nothing to do
        return;
    }

    auto editor = plugin.createEditor();
    if (editor != nullptr)
    {
        dc_log ("[PluginWindowManager] Created editor for: %s", plugin.getName().c_str());
        editors_[&plugin] = std::move (editor);
    }
}

void PluginWindowManager::closeEditorForPlugin (dc::PluginInstance* plugin)
{
    auto it = editors_.find (plugin);

    if (it != editors_.end())
        editors_.erase (it);
}

void PluginWindowManager::closeAll()
{
    editors_.clear();
}

} // namespace dc
