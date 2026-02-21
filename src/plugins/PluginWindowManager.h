#pragma once
#include <JuceHeader.h>

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
    juce::OwnedArray<PluginWindow> windows;
    std::map<juce::AudioPluginInstance*, PluginWindow*> pluginToWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindowManager)
};

} // namespace dc
