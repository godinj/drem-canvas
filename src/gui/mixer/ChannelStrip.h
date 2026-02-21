#pragma once
#include <JuceHeader.h>
#include "MeterComponent.h"
#include "model/Project.h"

namespace dc
{

class ChannelStrip : public juce::Component,
                     private juce::ValueTree::Listener
{
public:
    explicit ChannelStrip (const juce::ValueTree& trackState);
    ~ChannelStrip() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    MeterComponent& getMeter() { return meter; }

    // Callback when volume/pan/mute/solo changes
    std::function<void()> onStateChanged;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    juce::ValueTree trackState;

    juce::Slider fader;
    juce::Slider panKnob;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::Label nameLabel;
    MeterComponent meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace dc
