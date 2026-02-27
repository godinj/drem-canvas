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

    // External filter API (driven by VimEngine)
    void setSearchFilter (const juce::String& query);
    void clearSearchFilter();

    // Keyboard navigation
    int getNumPlugins() const;
    int getSelectedPluginIndex() const;
    void selectPlugin (int index);
    void moveSelection (int delta);
    void scrollByHalfPage (int direction); // +1 = down, -1 = up
    void confirmSelection();

    // Callback when a plugin is selected
    std::function<void (const juce::PluginDescription&)> onPluginSelected;

private:
    PluginManager& pluginManager;
    juce::ListBox pluginListBox;

    void rebuildFilteredList();

    class PluginListModel : public juce::ListBoxModel
    {
    public:
        explicit PluginListModel (juce::Array<juce::PluginDescription>& types)
            : filteredTypes (types) {}
        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics& g,
                               int width, int height, bool rowIsSelected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

        std::function<void (const juce::PluginDescription&)> onItemSelected;
    private:
        juce::Array<juce::PluginDescription>& filteredTypes;
    };

    juce::Array<juce::PluginDescription> filteredTypes;
    juce::String searchFilter;
    PluginListModel listModel;
    juce::TextButton scanButton { "Scan Plugins" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

} // namespace dc
