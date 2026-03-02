#include "PluginSlotList.h"
#include "gui/common/ColourBridge.h"

using dc::bridge::toJuce;

namespace dc
{

PluginSlotList::PluginSlotList (const PropertyTree& state)
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
        g.setColour (i % 2 == 0 ? toJuce (0xff2a2a3au) : toJuce (0xff262636u));
        g.fillRect (slotBounds);

        // Selected slot highlight — bright cursor bar + distinct background
        if (i == selectedSlotIndex)
        {
            g.setColour (toJuce (dc::Colour (0xff50c878u).withAlpha (0.35f)));
            g.fillRect (slotBounds);

            // Solid green cursor bar on left edge
            g.setColour (toJuce (0xff50c878u));
            g.fillRect (slotBounds.getX(), slotBounds.getY(), 3, slotBounds.getHeight());
        }

        // Slot number prefix
        std::string prefix = std::to_string (i + 1) + ": ";
        g.setFont (juce::Font (juce::FontOptions (11.0f)));

        if (i < numPlugins)
        {
            auto plugin = chain.getChild (i);
            bool enabled = plugin.getProperty (IDs::pluginEnabled, true).toBool();
            std::string name = plugin.getProperty (IDs::pluginName, "Plugin").toString();

            // Dim colour when bypassed
            g.setColour (enabled ? toJuce (0xffc0c0d0u) : toJuce (0xff606070u));
            g.drawText (prefix + name, slotBounds.reduced (4, 0), juce::Justification::centredLeft, true);
        }
        else if (i == selectedSlotIndex)
        {
            // "Add" slot indicator when selected
            g.setColour (toJuce (0xff50c878u));
            g.drawText (prefix + "[+]", slotBounds.reduced (4, 0), juce::Justification::centredLeft, true);
        }
        else
        {
            // Empty slot — show number
            g.setColour (toJuce (0xff484858u));
            g.drawText (prefix, slotBounds.reduced (4, 0), juce::Justification::centredLeft, true);
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

PropertyTree PluginSlotList::getPluginChain() const
{
    return trackState.getChildWithType (IDs::PLUGIN_CHAIN);
}

void PluginSlotList::childAdded (PropertyTree&, PropertyTree&)
{
    repaint();
}

void PluginSlotList::childRemoved (PropertyTree&, PropertyTree&, int)
{
    repaint();
}

void PluginSlotList::propertyChanged (PropertyTree&, PropertyId)
{
    repaint();
}

} // namespace dc
