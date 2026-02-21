#pragma once
#include <JuceHeader.h>
#include "plugins/PluginManager.h"

namespace dc
{

class BrowserPanel : public juce::Component
{
public:
    explicit BrowserPanel (PluginManager& pluginManager);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void refreshPluginList();

    // Callback when a plugin is selected
    std::function<void (const juce::PluginDescription&)> onPluginSelected;

private:
    PluginManager& pluginManager;
    juce::ListBox pluginListBox;

    class PluginListModel : public juce::ListBoxModel
    {
    public:
        explicit PluginListModel (PluginManager& pm) : manager (pm) {}
        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics& g,
                               int width, int height, bool rowIsSelected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

        std::function<void (const juce::PluginDescription&)> onItemSelected;
    private:
        PluginManager& manager;
    };

    PluginListModel listModel;
    juce::TextButton scanButton { "Scan Plugins" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

} // namespace dc
