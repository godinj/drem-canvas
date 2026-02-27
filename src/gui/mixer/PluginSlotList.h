#pragma once
#include <JuceHeader.h>
#include "model/Project.h"

namespace dc
{

class PluginSlotList : public juce::Component,
                       private juce::ValueTree::Listener
{
public:
    explicit PluginSlotList (const juce::ValueTree& trackState);
    ~PluginSlotList() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    static constexpr int slotHeight = 18;
    static constexpr int maxVisibleSlots = 4;
    static constexpr int preferredHeight = slotHeight * maxVisibleSlots;

    // Vim slot selection highlight
    void setSelectedSlotIndex (int index);

    // Callbacks
    std::function<void (int pluginIndex)> onPluginClicked;
    std::function<void (int pluginIndex)> onPluginBypassToggled;
    std::function<void (int pluginIndex)> onPluginRemoveRequested;

private:
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    int getSlotIndexAt (int y) const;
    juce::ValueTree getPluginChain() const;

    juce::ValueTree trackState;
    int selectedSlotIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginSlotList)
};

} // namespace dc
