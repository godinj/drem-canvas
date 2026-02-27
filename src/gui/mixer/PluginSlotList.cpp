#include "PluginSlotList.h"

namespace dc
{

PluginSlotList::PluginSlotList (const juce::ValueTree& state)
    : trackState (state)
{
    trackState.addListener (this);
}

PluginSlotList::~PluginSlotList()
{
    trackState.removeListener (this);
}

void PluginSlotList::setSelectedSlotIndex (int index)
{
    if (selectedSlotIndex != index)
    {
        selectedSlotIndex = index;
        repaint();
    }
}

void PluginSlotList::paint (juce::Graphics& g)
{
    auto chain = getPluginChain();
    int numPlugins = chain.isValid() ? chain.getNumChildren() : 0;

    for (int i = 0; i < maxVisibleSlots; ++i)
    {
        auto slotBounds = juce::Rectangle<int> (0, i * slotHeight, getWidth(), slotHeight);

        // Background
        g.setColour (i % 2 == 0 ? juce::Colour (0xff2a2a3a) : juce::Colour (0xff262636));
        g.fillRect (slotBounds);

        // Selected slot highlight
        if (i == selectedSlotIndex)
        {
            g.setColour (juce::Colour (0xff50c878).withAlpha (0.15f));
            g.fillRect (slotBounds);
            g.setColour (juce::Colour (0xff50c878));
            g.drawRect (slotBounds, 1);
        }

        if (i < numPlugins)
        {
            auto plugin = chain.getChild (i);
            bool enabled = plugin.getProperty (IDs::pluginEnabled, true);
            juce::String name = plugin.getProperty (IDs::pluginName, "Plugin").toString();

            // Dim colour when bypassed
            g.setColour (enabled ? juce::Colour (0xffc0c0d0) : juce::Colour (0xff606070));
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (name, slotBounds.reduced (4, 0), juce::Justification::centredLeft, true);
        }
        else if (i == selectedSlotIndex)
        {
            // "Add" slot indicator when selected
            g.setColour (juce::Colour (0xff50c878));
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText ("[+]", slotBounds.reduced (4, 0), juce::Justification::centredLeft, true);
        }
    }
}

void PluginSlotList::mouseDown (const juce::MouseEvent& e)
{
    int index = getSlotIndexAt (e.y);
    auto chain = getPluginChain();
    int numPlugins = chain.isValid() ? chain.getNumChildren() : 0;

    if (index < 0 || index >= numPlugins)
        return;

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Toggle Bypass");
        menu.addItem (2, "Remove");

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [this, index] (int result)
            {
                if (result == 1 && onPluginBypassToggled)
                    onPluginBypassToggled (index);
                else if (result == 2 && onPluginRemoveRequested)
                    onPluginRemoveRequested (index);
            });
    }
    else
    {
        if (onPluginClicked)
            onPluginClicked (index);
    }
}

int PluginSlotList::getSlotIndexAt (int y) const
{
    return y / slotHeight;
}

juce::ValueTree PluginSlotList::getPluginChain() const
{
    return trackState.getChildWithName (IDs::PLUGIN_CHAIN);
}

void PluginSlotList::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    repaint();
}

void PluginSlotList::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    repaint();
}

void PluginSlotList::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    repaint();
}

} // namespace dc
