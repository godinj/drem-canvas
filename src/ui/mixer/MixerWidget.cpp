#include "MixerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "model/Track.h"
#include "model/MixerState.h"
#include "vim/VimContext.h"
#include <cmath>
#include <string>

namespace dc
{
namespace ui
{

static float dbToLinear (float db) { return std::pow (10.0f, db / 20.0f); }
static float linearToDb (float linear) { return linear <= 0.0f ? -60.0f : 20.0f * std::log10 (linear); }

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
            std::string name = pluginState.getProperty (IDs::pluginName).getStringOr ("Plugin");
            bool enabled = track.isPluginEnabled (p);
            slots.push_back ({ name, ! enabled });
        }
        strip->getPluginSlots().setSlots (slots);

        int trackIndex = i;

        // Wire fader/pan/mute/solo callbacks to model
        strip->onVolumeChange = [this, trackIndex] (double dbValue)
        {
            Track track (project.getTrack (trackIndex));
            track.setVolume (dbToLinear (static_cast<float> (dbValue)));
        };
        strip->onPanChange = [this, trackIndex] (double value)
        {
            Track track (project.getTrack (trackIndex));
            track.setPan (static_cast<float> (value));
        };
        strip->onMuteChange = [this, trackIndex] (bool muted)
        {
            Track track (project.getTrack (trackIndex));
            track.setMuted (muted);
        };
        strip->onSoloChange = [this, trackIndex] (bool soloed)
        {
            Track track (project.getTrack (trackIndex));
            track.setSolo (soloed);
        };

        // Wire plugin slot click to open editor
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
        PropertyTree masterState (IDs::TRACK);
        masterState.setProperty (IDs::name, Variant (std::string ("Master")));
        // Initialise master volume from model so the fader reflects saved state
        MixerState mixer (project);
        masterState.setProperty (IDs::volume, Variant (static_cast<double> (mixer.getMasterVolume())));
        masterStrip = std::make_unique<ChannelStripWidget> (masterState);
        masterStrip->onVolumeChange = [this] (double dbValue)
        {
            float linear = dbToLinear (static_cast<float> (dbValue));
            MixerState mixer (project);
            mixer.setMasterVolume (linear);
            if (onMasterVolumeChange)
                onMasterVolumeChange (linear);
        };
        masterStrip->onMuteChange = [this] (bool muted)
        {
            static const PropertyId masterMuteId ("masterMute");
            project.getState().setProperty (masterMuteId, Variant (muted));
            if (onMasterMuteChange)
                onMasterMuteChange (muted);
        };
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

void MixerWidget::setSelectedPluginSlot (int slotIndex)
{
    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setSelectedPluginSlot (static_cast<int> (i) == selectedStripIndex ? slotIndex : -1);

    if (masterStrip)
        masterStrip->setSelectedPluginSlot (selectedStripIndex == static_cast<int> (strips.size()) ? slotIndex : -1);
}

void MixerWidget::propertyChanged (PropertyTree& tree, PropertyId property)
{
    if (tree.getType() == IDs::TRACK)
    {
        for (auto& strip : strips)
            strip->syncFromTrackState();
    }

    if (tree.getType() == IDs::PROJECT && masterStrip)
    {
        static const PropertyId masterVolumeId ("masterVolume");
        static const PropertyId masterMuteId ("masterMute");
        if (property == masterVolumeId)
        {
            MixerState mixer (project);
            masterStrip->getFader().setValue (
                static_cast<double> (linearToDb (mixer.getMasterVolume())));
        }
        if (property == masterMuteId)
        {
            masterStrip->getMuteButton().setToggleState (
                tree.getProperty (masterMuteId).getBoolOr (false));
        }
    }
}

void MixerWidget::syncStripValues()
{
    for (auto& strip : strips)
        strip->syncFromTrackState();

    if (masterStrip)
    {
        MixerState mixer (project);
        masterStrip->getFader().setValue (
            static_cast<double> (linearToDb (mixer.getMasterVolume())));
    }
}

void MixerWidget::childAdded (PropertyTree&, PropertyTree&)
{
    rebuildStrips();
}

void MixerWidget::childRemoved (PropertyTree&, PropertyTree&, int)
{
    rebuildStrips();
}

} // namespace ui
} // namespace dc
