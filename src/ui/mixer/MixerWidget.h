#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ScrollViewWidget.h"
#include "ChannelStripWidget.h"
#include "model/Project.h"
#include "vim/VimContext.h"
#include <vector>
#include <memory>

namespace dc
{
namespace ui
{

class MixerWidget : public gfx::Widget
{
public:
    explicit MixerWidget (Project& project);
    ~MixerWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void resized() override;

    void rebuildStrips();
    void setActiveContext (bool active);
    void setSelectedStripIndex (int index);
    void setMixerFocus (VimContext::MixerFocus focus);
    void setSelectedPluginSlot (int slotIndex);

    // Access strips for meter wiring
    std::vector<std::unique_ptr<ChannelStripWidget>>& getStrips() { return strips; }
    ChannelStripWidget* getMasterStrip() { return masterStrip.get(); }

    // Callback: (trackIndex, pluginIndex) when a plugin slot is clicked
    std::function<void (int, int)> onPluginClicked;

    // Callbacks for master strip changes (linear gain, mute state)
    std::function<void (float)> onMasterVolumeChange;
    std::function<void (bool)> onMasterMuteChange;

    // PropertyTree::Listener
    void childAdded (PropertyTree&, PropertyTree&) override;
    void childRemoved (PropertyTree&, PropertyTree&, int) override;
    void propertyChanged (PropertyTree&, PropertyId) override;

    void syncStripValues();

private:
    Project& project;
    gfx::ScrollViewWidget scrollView;
    gfx::Widget stripContainer;
    std::vector<std::unique_ptr<ChannelStripWidget>> strips;
    std::unique_ptr<ChannelStripWidget> masterStrip;
    bool activeContext = false;
    int selectedStripIndex = -1;
};

} // namespace ui
} // namespace dc
