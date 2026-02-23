#include "ArrangementWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "model/Track.h"

namespace dc
{
namespace ui
{

ArrangementWidget::ArrangementWidget (Project& proj, TransportController& transport,
                                      Arrangement& arr, VimContext& vc)
    : project (proj), transportController (transport), arrangement (arr), vimContext (vc)
{
    addChild (&timeRuler);
    addChild (&scrollView);
    scrollView.setContentWidget (&trackContainer);

    // Seek callback
    timeRuler.onSeek = [this] (double timeInSeconds)
    {
        double sr = transportController.getSampleRate();
        transportController.setPositionInSamples (static_cast<int64_t> (timeInSeconds * sr));
    };

    // Listen to model changes
    project.getState().addListener (this);

    setAnimating (true);
    rebuildTrackLanes();
}

ArrangementWidget::~ArrangementWidget()
{
    project.getState().removeListener (this);
}

void ArrangementWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void ArrangementWidget::paintOverChildren (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();

    // Draw playhead cursor (red vertical line) — over everything
    double sr = transportController.getSampleRate();
    if (sr <= 0.0) return;

    int64_t posInSamples = transportController.getPositionInSamples();
    double posInSeconds = static_cast<double> (posInSamples) / sr;

    float cursorX = static_cast<float> (posInSeconds * pixelsPerSecond)
                  + theme.headerWidth - scrollView.getScrollOffsetX();

    if (cursorX >= theme.headerWidth && cursorX <= getWidth())
    {
        canvas.drawLine (cursorX, rulerHeight, cursorX, getHeight(),
                         theme.playhead, 2.0f);
    }
}

void ArrangementWidget::resized()
{
    float w = getWidth();
    float h = getHeight();

    timeRuler.setBounds (0, 0, w, rulerHeight);
    scrollView.setBounds (0, rulerHeight, w, h - rulerHeight);

    float contentWidth = std::max (w, 10000.0f);
    float contentHeight = static_cast<float> (trackLanes.size()) * trackHeight;
    scrollView.setContentSize (contentWidth, contentHeight);

    for (size_t i = 0; i < trackLanes.size(); ++i)
    {
        trackLanes[i]->setBounds (0, static_cast<float> (i) * trackHeight,
                                   contentWidth, trackHeight);
    }
}

void ArrangementWidget::animationTick (double /*timestampMs*/)
{
    // Sync scroll offset to time ruler
    timeRuler.setScrollOffset (static_cast<double> (scrollView.getScrollOffsetX()));

    // Repaint for playhead animation
    repaint();
}

void ArrangementWidget::rebuildTrackLanes()
{
    for (auto& lane : trackLanes)
        trackContainer.removeChild (lane.get());
    trackLanes.clear();

    double sr = transportController.getSampleRate();
    if (sr <= 0.0) sr = 44100.0;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        auto trackState = project.getTrack (i);
        auto lane = std::make_unique<TrackLaneWidget> (trackState);
        lane->setPixelsPerSecond (pixelsPerSecond);
        lane->setSampleRate (sr);
        trackContainer.addChild (lane.get());
        trackLanes.push_back (std::move (lane));
    }

    updateSelectionVisuals();
    resized();
}

void ArrangementWidget::updateSelectionVisuals()
{
    int selectedTrack = arrangement.getSelectedTrackIndex();
    int selectedClip = vimContext.getSelectedClipIndex();

    for (size_t i = 0; i < trackLanes.size(); ++i)
    {
        bool isSelected = static_cast<int> (i) == selectedTrack;
        trackLanes[i]->setSelected (isSelected);
        trackLanes[i]->setSelectedClipIndex (isSelected ? selectedClip : -1);
    }
}

// ─── VimEngine::Listener ─────────────────────────────────────

void ArrangementWidget::vimModeChanged (VimEngine::Mode)
{
    repaint();
}

void ArrangementWidget::vimContextChanged()
{
    updateSelectionVisuals();
    repaint();
}

// ─── ValueTree::Listener ─────────────────────────────────────

void ArrangementWidget::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    rebuildTrackLanes();
}

void ArrangementWidget::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    rebuildTrackLanes();
}

} // namespace ui
} // namespace dc
