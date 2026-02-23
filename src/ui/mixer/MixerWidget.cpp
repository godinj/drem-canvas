#include "MixerWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

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
