#pragma once
#include <JuceHeader.h>
#include "MeterComponent.h"
#include "PluginSlotList.h"
#include "model/Project.h"
#include "utils/UndoSystem.h"
#include "vim/VimContext.h"

namespace dc
{

class ChannelStrip : public juce::Component,
                     private juce::ValueTree::Listener
{
public:
    explicit ChannelStrip (const juce::ValueTree& trackState, UndoSystem* undoSystem = nullptr);
    ~ChannelStrip() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

    MeterComponent& getMeter() { return meter; }

    // Vim cursor selection
    void setSelected (bool shouldBeSelected);

    // Mixer parameter focus
    void setMixerFocus (VimContext::MixerFocus focus);

    // Callback when volume/pan/mute/solo changes
    std::function<void()> onStateChanged;

    // Plugin callbacks (wired by MainComponent)
    std::function<void (int pluginIndex)> onPluginClicked;
    std::function<void (int pluginIndex)> onPluginBypassToggled;
    std::function<void (int pluginIndex)> onPluginRemoveRequested;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    juce::ValueTree trackState;
    UndoSystem* undoSystem = nullptr;

    juce::Slider fader;
    juce::Slider panKnob;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::Label nameLabel;
    MeterComponent meter;
    PluginSlotList pluginSlotList;

    bool selected = false;
    VimContext::MixerFocus currentFocus = VimContext::FocusNone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace dc
