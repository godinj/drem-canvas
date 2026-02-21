#include "BrowserPanel.h"

namespace dc
{

//==============================================================================
// BrowserPanel
//==============================================================================

BrowserPanel::BrowserPanel (PluginManager& pm)
    : pluginManager (pm),
      listModel (pm)
{
    pluginListBox.setModel (&listModel);
    pluginListBox.setRowHeight (24);
    pluginListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff252535));
    addAndMakeVisible (pluginListBox);

    scanButton.onClick = [this]()
    {
        pluginManager.scanDefaultPaths();
        refreshPluginList();
    };
    addAndMakeVisible (scanButton);

    listModel.onItemSelected = [this] (const juce::PluginDescription& desc)
    {
        if (onPluginSelected)
            onPluginSelected (desc);
    };
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff252535));
}

void BrowserPanel::resized()
{
    auto area = getLocalBounds();

    scanButton.setBounds (area.removeFromTop (30).reduced (4, 2));
    pluginListBox.setBounds (area);
}

void BrowserPanel::refreshPluginList()
{
    pluginListBox.updateContent();
    pluginListBox.repaint();
}

//==============================================================================
// PluginListModel
//==============================================================================

int BrowserPanel::PluginListModel::getNumRows()
{
    return manager.getKnownPlugins().getNumTypes();
}

void BrowserPanel::PluginListModel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                       int width, int height,
                                                       bool rowIsSelected)
{
    auto types = manager.getKnownPlugins().getTypes();

    if (rowNumber < 0 || rowNumber >= static_cast<int> (types.size()))
        return;

    const auto& desc = types[static_cast<size_t> (rowNumber)];

    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff3a3a5a));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (14.0f));

    auto textArea = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);

    // Plugin name on the left
    g.drawText (desc.name, textArea, juce::Justification::centredLeft, true);

    // Manufacturer on the right
    g.setColour (juce::Colours::lightgrey);
    g.drawText (desc.manufacturerName, textArea, juce::Justification::centredRight, true);
}

void BrowserPanel::PluginListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    auto types = manager.getKnownPlugins().getTypes();

    if (row >= 0 && row < static_cast<int> (types.size()))
    {
        const auto& desc = types[static_cast<size_t> (row)];

        if (onItemSelected)
            onItemSelected (desc);
    }
}

} // namespace dc
