#include "ChannelStripWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

ChannelStripWidget::ChannelStripWidget (const juce::ValueTree& state)
    : trackState (state),
      panKnob (gfx::SliderWidget::Rotary),
      muteButton ("M"),
      soloButton ("S"),
      fader (gfx::SliderWidget::LinearVertical)
{
    juce::String name = trackState.getProperty ("name", "Track");
    nameLabel.setText (name.toStdString());
    nameLabel.setAlignment (gfx::LabelWidget::Centre);
    nameLabel.setFontSize (11.0f);

    muteButton.setToggleable (true);
    soloButton.setToggleable (true);

    fader.setRange (-60.0, 6.0);
    fader.setValue (0.0);

    panKnob.setRange (-1.0, 1.0);
    panKnob.setValue (0.0);

    fader.onValueChange = [this] (double value)
    {
        if (onVolumeChange) onVolumeChange (value);
    };

    panKnob.onValueChange = [this] (double value)
    {
        if (onPanChange) onPanChange (value);
    };

    addChild (&nameLabel);
    addChild (&pluginSlots);
    addChild (&meter);
    addChild (&panKnob);
    addChild (&muteButton);
    addChild (&soloButton);
    addChild (&fader);
}

void ChannelStripWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    canvas.fillRect (Rect (0, 0, w, h), theme.widgetBackground);

    if (selected)
        canvas.strokeRect (Rect (0, 0, w, h), theme.selection, 2.0f);

    // Right separator
    canvas.drawLine (w - 1.0f, 0, w - 1.0f, h, theme.outlineColor, 1.0f);
}

void ChannelStripWidget::resized()
{
    float w = getWidth();
    float margin = 4.0f;

    float y = margin;

    // Name label at top
    nameLabel.setBounds (margin, y, w - 2.0f * margin, 18.0f);
    y += 20.0f;

    // Plugin slots
    pluginSlots.setBounds (margin, y, w - 2.0f * margin, 60.0f);
    y += 64.0f;

    // Meter (takes available space with fader)
    float meterHeight = getHeight() - y - 100.0f;
    float meterWidth = 20.0f;
    float faderWidth = w - meterWidth - 3.0f * margin;

    meter.setBounds (margin, y, meterWidth, meterHeight);
    fader.setBounds (margin + meterWidth + margin, y, faderWidth, meterHeight);
    y += meterHeight + margin;

    // Pan knob
    float knobSize = 30.0f;
    panKnob.setBounds ((w - knobSize) * 0.5f, y, knobSize, knobSize);
    y += knobSize + margin;

    // Mute/Solo buttons
    float buttonW = (w - 3.0f * margin) * 0.5f;
    muteButton.setBounds (margin, y, buttonW, 22.0f);
    soloButton.setBounds (margin + buttonW + margin, y, buttonW, 22.0f);
}

void ChannelStripWidget::paintOverChildren (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();

    if (! selected || currentFocus == VimContext::FocusNone)
        return;

    gfx::Widget* focusedWidget = nullptr;

    switch (currentFocus)
    {
        case VimContext::FocusVolume:  focusedWidget = &fader; break;
        case VimContext::FocusPan:     focusedWidget = &panKnob; break;
        case VimContext::FocusPlugins: focusedWidget = &pluginSlots; break;
        default: break;
    }

    if (focusedWidget != nullptr)
    {
        Rect focusBounds = focusedWidget->getBounds().reduced (-2.0f);

        if (currentFocus == VimContext::FocusPlugins)
        {
            // Plugins: only draw a subtle border — individual slot highlights
            // inside PluginSlotListWidget handle per-slot selection feedback
            canvas.strokeRect (focusBounds, theme.selection.withAlpha ((uint8_t) 102), 1.0f);
        }
        else
        {
            // Volume / Pan: full highlight
            canvas.fillRoundedRect (focusBounds, 2.0f, theme.selection.withAlpha ((uint8_t) 46));
            canvas.strokeRect (focusBounds, theme.selection, 1.5f);
        }
    }
}

void ChannelStripWidget::setSelected (bool sel)
{
    if (selected != sel)
    {
        selected = sel;
        repaint();
    }
}

void ChannelStripWidget::setMixerFocus (VimContext::MixerFocus focus)
{
    if (currentFocus != focus)
    {
        currentFocus = focus;
        repaint();
    }
}

void ChannelStripWidget::setSelectedPluginSlot (int slotIndex)
{
    pluginSlots.setSelectedSlotIndex (slotIndex);
}

} // namespace ui
} // namespace dc
