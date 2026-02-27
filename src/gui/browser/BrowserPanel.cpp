#include "BrowserPanel.h"

namespace dc
{

//==============================================================================
// BrowserPanel
//==============================================================================

BrowserPanel::BrowserPanel (PluginManager& pm)
    : pluginManager (pm),
      listModel (filteredTypes)
{
    pluginListBox.setModel (&listModel);
    pluginListBox.setRowHeight (24);
    pluginListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff252535));
    pluginListBox.setWantsKeyboardFocus (false);
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
    searchFilter.clear();
    rebuildFilteredList();
}

void BrowserPanel::rebuildFilteredList()
{
    filteredTypes.clear();
    auto allTypes = pluginManager.getKnownPlugins().getTypes();

    if (searchFilter.isEmpty())
    {
        for (const auto& t : allTypes)
            filteredTypes.add (t);
    }
    else
    {
        auto queryLower = searchFilter.toLowerCase();
        for (const auto& t : allTypes)
        {
            if (t.name.toLowerCase().contains (queryLower)
                || t.manufacturerName.toLowerCase().contains (queryLower))
                filteredTypes.add (t);
        }
    }

    pluginListBox.updateContent();
    pluginListBox.repaint();
}

void BrowserPanel::setSearchFilter (const juce::String& query)
{
    searchFilter = query;
    rebuildFilteredList();
    if (filteredTypes.size() > 0)
        selectPlugin (0);
}

void BrowserPanel::clearSearchFilter()
{
    searchFilter.clear();
    rebuildFilteredList();
    if (filteredTypes.size() > 0)
        selectPlugin (0);
}

int BrowserPanel::getNumPlugins() const
{
    return filteredTypes.size();
}

int BrowserPanel::getSelectedPluginIndex() const
{
    return pluginListBox.getSelectedRow();
}

void BrowserPanel::selectPlugin (int index)
{
    int numRows = getNumPlugins();
    if (numRows == 0) return;

    index = ((index % numRows) + numRows) % numRows;
    pluginListBox.selectRow (index);
    pluginListBox.scrollToEnsureRowIsOnscreen (index);
}

void BrowserPanel::moveSelection (int delta)
{
    int current = pluginListBox.getSelectedRow();
    if (current < 0) current = 0;
    selectPlugin (current + delta);
}

void BrowserPanel::scrollByHalfPage (int direction)
{
    int visibleRows = pluginListBox.getHeight() / pluginListBox.getRowHeight();
    int halfPage = std::max (1, visibleRows / 2);
    moveSelection (direction * halfPage);
}

void BrowserPanel::confirmSelection()
{
    int row = pluginListBox.getSelectedRow();

    if (row >= 0 && row < filteredTypes.size())
    {
        if (onPluginSelected)
            onPluginSelected (filteredTypes[row]);
    }
}

//==============================================================================
// PluginListModel
//==============================================================================

int BrowserPanel::PluginListModel::getNumRows()
{
    return filteredTypes.size();
}

void BrowserPanel::PluginListModel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                       int width, int height,
                                                       bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= filteredTypes.size())
        return;

    const auto& desc = filteredTypes[rowNumber];

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
    if (row >= 0 && row < filteredTypes.size())
    {
        const auto& desc = filteredTypes[row];

        if (onItemSelected)
            onItemSelected (desc);
    }
}

} // namespace dc
