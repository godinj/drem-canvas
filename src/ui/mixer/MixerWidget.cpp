#include "MixerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "model/Track.h"
#include "vim/VimContext.h"

namespace dc
{
namespace ui
{

MixerWidget::MixerWidget (Project& proj)
    : project (proj)
{
    addChild (&scrollView);
    scrollView.setContentWidget (&stripContainer);
    scrollView.setShowVerticalScrollbar (false);

    project.getState().addListener (this);
    rebuildStrips();
}

MixerWidget::~MixerWidget()
{
    project.getState().removeListener (this);
}

void MixerWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void MixerWidget::resized()
{
    float w = getWidth();
    float h = getHeight();
    auto& theme = gfx::Theme::getDefault();
    float masterWidth = theme.stripWidth + 10.0f;

    // Master strip on the right
    if (masterStrip)
        masterStrip->setBounds (w - masterWidth, 0, masterWidth, h);

    // Scroll view for track strips
    scrollView.setBounds (0, 0, w - masterWidth - 2.0f, h);

    // Layout strips
    float stripW = theme.stripWidth;
    float contentWidth = static_cast<float> (strips.size()) * stripW;
    scrollView.setContentSize (contentWidth, h);

    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (static_cast<float> (i) * stripW, 0, stripW, h);
}

void MixerWidget::rebuildStrips()
{
    for (auto& strip : strips)
        stripContainer.removeChild (strip.get());
    strips.clear();

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        auto trackState = project.getTrack (i);
        auto strip = std::make_unique<ChannelStripWidget> (trackState);

        // Populate plugin slots from model
        Track track (trackState);
        std::vector<PluginSlotListWidget::PluginSlot> slots;
        for (int p = 0; p < track.getNumPlugins(); ++p)
        {
            auto pluginState = track.getPlugin (p);
            juce::String name = pluginState.getProperty (IDs::pluginName, "Plugin");
            bool enabled = track.isPluginEnabled (p);
            slots.push_back ({ name.toStdString(), ! enabled });
        }
        strip->getPluginSlots().setSlots (slots);

        // Wire plugin slot click to open editor
        int trackIndex = i;
        strip->getPluginSlots().onSlotClicked = [this, trackIndex] (int pluginIndex)
        {
            if (onPluginClicked)
                onPluginClicked (trackIndex, pluginIndex);
        };

        stripContainer.addChild (strip.get());
        strips.push_back (std::move (strip));
    }

    // Master strip
    if (!masterStrip)
    {
        juce::ValueTree masterState ("TRACK");
        masterState.setProperty ("name", "Master", nullptr);
        masterStrip = std::make_unique<ChannelStripWidget> (masterState);
        addChild (masterStrip.get());
    }

    resized();
}

void MixerWidget::paintOverChildren (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();

    if (activeContext)
    {
        canvas.fillRect (Rect (0, 0, getWidth(), 2.0f), theme.selection);
    }
    else
    {
        canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), Color (0, 0, 0, 40));
    }
}

void MixerWidget::setActiveContext (bool active)
{
    if (activeContext != active)
    {
        activeContext = active;
        repaint();
    }
}

void MixerWidget::setSelectedStripIndex (int index)
{
    if (selectedStripIndex == index)
        return;

    selectedStripIndex = index;

    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setSelected (static_cast<int> (i) == index);

    if (masterStrip)
        masterStrip->setSelected (index == static_cast<int> (strips.size()));
}

void MixerWidget::setMixerFocus (VimContext::MixerFocus focus)
{
    for (auto& strip : strips)
        strip->setMixerFocus (focus);

    if (masterStrip)
        masterStrip->setMixerFocus (focus);
}

void MixerWidget::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    rebuildStrips();
}

void MixerWidget::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    rebuildStrips();
}

} // namespace ui
} // namespace dc
