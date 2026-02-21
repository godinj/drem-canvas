#pragma once
#include <JuceHeader.h>
#include "ChannelStrip.h"
#include "model/Project.h"
#include "engine/MixBusProcessor.h"

namespace dc
{

class MixerPanel : public juce::Component,
                   private juce::ValueTree::Listener
{
public:
    MixerPanel (Project& project, MixBusProcessor& masterBus);
    ~MixerPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void rebuildStrips();

    // Vim cursor selection — index into strips[], or strips.size() for master
    void setSelectedStripIndex (int index);

    // Call this to wire meter sources from track processors
    std::function<void (int trackIndex, ChannelStrip& strip)> onWireMeter;

private:
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

    Project& project;
    MixBusProcessor& masterBus;

    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> masterStrip;
    MeterComponent masterMeter;

    int selectedStripIndex = -1;

    static constexpr int stripWidth = 80;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPanel)
};

} // namespace dc
