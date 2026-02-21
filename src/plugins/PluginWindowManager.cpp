#include "PluginWindowManager.h"

namespace dc
{

//==============================================================================
// PluginWindow
//==============================================================================

PluginWindow::PluginWindow (juce::AudioProcessorEditor* editor, const juce::String& name)
    : juce::DocumentWindow (name, juce::Colours::darkgrey, juce::DocumentWindow::allButtons)
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
        auto* window = new PluginWindow (editor, plugin.getName());
        windows.add (window);
        pluginToWindow[&plugin] = window;
    }
}

void PluginWindowManager::closeEditorForPlugin (juce::AudioPluginInstance* plugin)
{
    auto it = pluginToWindow.find (plugin);

    if (it != pluginToWindow.end())
    {
        auto* window = it->second;
        pluginToWindow.erase (it);
        windows.removeObject (window, true);
    }
}

void PluginWindowManager::closeAll()
{
    pluginToWindow.clear();
    windows.clear (true);
}

} // namespace dc
