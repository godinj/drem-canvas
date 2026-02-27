#pragma once
#include <JuceHeader.h>
#include "ChannelStrip.h"
#include "model/Project.h"
#include "engine/MixBusProcessor.h"
#include "vim/VimContext.h"

namespace dc
{

class MixerPanel : public juce::Component,
                   private juce::ValueTree::Listener
{
public:
    MixerPanel (Project& project, MixBusProcessor& masterBus, UndoSystem* undoSystem = nullptr);
    ~MixerPanel() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

    void rebuildStrips();

    // Vim cursor selection — index into strips[], or strips.size() for master
    void setSelectedStripIndex (int index);

    // Active context indicator
    void setActiveContext (bool active);

    // Mixer parameter focus
    void setMixerFocus (VimContext::MixerFocus focus);

    // Plugin slot selection (only highlighted on the selected strip)
    void setSelectedPluginSlot (int slotIndex);

    // Call this to wire meter sources from track processors
    std::function<void (int trackIndex, ChannelStrip& strip)> onWireMeter;

private:
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    Project& project;
    MixBusProcessor& masterBus;
    UndoSystem* undoSystem = nullptr;

    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> masterStrip;
    juce::ValueTree masterState;
    MeterComponent masterMeter;

    int selectedStripIndex = -1;
    bool activeContext = false;

    static constexpr int stripWidth = 80;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};

} // namespace dc
