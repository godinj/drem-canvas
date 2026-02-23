#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/SliderWidget.h"
#include "graphics/widgets/ButtonWidget.h"
#include "graphics/widgets/LabelWidget.h"
#include "MeterWidget.h"
#include "PluginSlotListWidget.h"
#include <JuceHeader.h>
#include <functional>

namespace dc
{
namespace ui
{

class ChannelStripWidget : public gfx::Widget
{
public:
    explicit ChannelStripWidget (const juce::ValueTree& trackState);

    void paint (gfx::Canvas& canvas) override;
    void resized() override;

    void setSelected (bool sel);
    bool isSelected() const { return selected; }

    MeterWidget& getMeter() { return meter; }
    gfx::SliderWidget& getFader() { return fader; }
    gfx::SliderWidget& getPanKnob() { return panKnob; }

    std::function<void (double)> onVolumeChange;
    std::function<void (double)> onPanChange;

private:
    juce::ValueTree trackState;
    bool selected = false;

    gfx::LabelWidget nameLabel;
    PluginSlotListWidget pluginSlots;
    MeterWidget meter;
    gfx::SliderWidget panKnob;
    gfx::ButtonWidget muteButton;
    gfx::ButtonWidget soloButton;
    gfx::SliderWidget fader;
};

} // namespace ui
} // namespace dc
