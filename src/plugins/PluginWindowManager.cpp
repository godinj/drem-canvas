#include "PluginWindowManager.h"
#include "gui/common/ColourBridge.h"

namespace dc
{

//==============================================================================
// PluginWindow
//==============================================================================

PluginWindow::PluginWindow (juce::AudioProcessorEditor* editor, const juce::String& name)
    : juce::DocumentWindow (name, dc::bridge::toJuce (dc::Colours::darkgrey), juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setContentOwned (editor, true);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void PluginWindow::closeButtonPressed()
{
    setVisible (false);
}

//==============================================================================
// PluginWindowManager
//==============================================================================

PluginWindowManager::PluginWindowManager() = default;

PluginWindowManager::~PluginWindowManager()
{
    closeAll();
}

void PluginWindowManager::showEditorForPlugin (juce::AudioPluginInstance& plugin)
{
    auto it = pluginToWindow.find (&plugin);

    if (it != pluginToWindow.end())
    {
        it->second->setVisible (true);
        it->second->toFront (true);
        return;
    }

    if (auto* editor = plugin.createEditorIfNeeded())
    {
        auto window = std::make_unique<PluginWindow> (editor, plugin.getName());
        pluginToWindow[&plugin] = window.get();
        windows.push_back (std::move (window));
    }
}

void PluginWindowManager::closeEditorForPlugin (juce::AudioPluginInstance* plugin)
{
    auto it = pluginToWindow.find (plugin);

    if (it != pluginToWindow.end())
    {
        auto* window = it->second;
        pluginToWindow.erase (it);
        windows.erase (
            std::remove_if (windows.begin(), windows.end(),
                            [window] (const std::unique_ptr<PluginWindow>& w) { return w.get() == window; }),
            windows.end());
    }
}

void PluginWindowManager::closeAll()
{
    pluginToWindow.clear();
    windows.clear();
}

} // namespace dc
