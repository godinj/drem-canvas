#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace dc
{

class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioProcessorEditor* editor, const juce::String& name);
    void closeButtonPressed() override;
};

class PluginWindowManager
{
public:
    PluginWindowManager();
    ~PluginWindowManager();

    void showEditorForPlugin (juce::AudioPluginInstance& plugin);
    void closeEditorForPlugin (juce::AudioPluginInstance* plugin);
    void closeAll();

private:
    std::vector<std::unique_ptr<PluginWindow>> windows;
    std::map<juce::AudioPluginInstance*, PluginWindow*> pluginToWindow;

    PluginWindowManager (const PluginWindowManager&) = delete;
    PluginWindowManager& operator= (const PluginWindowManager&) = delete;
};

} // namespace dc
