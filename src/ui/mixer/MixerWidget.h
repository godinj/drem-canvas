#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ScrollViewWidget.h"
#include "ChannelStripWidget.h"
#include "model/Project.h"
#include <JuceHeader.h>
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
    void resized() override;

    void rebuildStrips();

    // ValueTree::Listener
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

private:
    Project& project;
    gfx::ScrollViewWidget scrollView;
    gfx::Widget stripContainer;
    std::vector<std::unique_ptr<ChannelStripWidget>> strips;
    std::unique_ptr<ChannelStripWidget> masterStrip;
};

} // namespace ui
} // namespace dc
