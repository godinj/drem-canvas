#include "ArrangementWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "model/Track.h"

namespace dc
{
namespace ui
{

ArrangementWidget::ArrangementWidget (Project& proj, TransportController& transport,
                                      Arrangement& arr, VimContext& vc,
                                      const TempoMap& tempo, GridSystem& gs)
    : project (proj), transportController (transport), arrangement (arr), vimContext (vc),
      tempoMap (tempo), gridSystem (gs), timeRuler (tempo)
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

    // Active context indicator
    if (activeContext)
    {
        // Green top bar
        canvas.fillRect (Rect (0, 0, getWidth(), 2.0f), theme.selection);
    }
    else
    {
        // Dark overlay for inactive panel
        canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), Color (0, 0, 0, 40));
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
    // Deferred rebuild — coalesces multiple ValueTree changes into a single rebuild
    if (needsRebuild)
    {
        needsRebuild = false;
        rebuildTrackLanes();
    }

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
        lane->setTempo (tempoMap.getTempo());
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
    auto& visualSel = vimContext.getVisualSelection();

    double sr = transportController.getSampleRate();
    int64_t gridUnit = (sr > 0.0) ? gridSystem.getGridUnitInSamples (sr) : 0;
    int64_t gridPos = vimContext.getGridCursorPosition();

    for (size_t i = 0; i < trackLanes.size(); ++i)
    {
        bool isSelected = static_cast<int> (i) == selectedTrack;
        trackLanes[i]->setSelected (isSelected);
        trackLanes[i]->setSelectedClipIndex (isSelected ? selectedClip : -1);
        trackLanes[i]->setVisualSelection (visualSel, static_cast<int> (i));
        trackLanes[i]->setGridCursorPosition (isSelected ? gridPos : -1);
        trackLanes[i]->setGridUnitInSamples (gridUnit);

        // Grid visual selection
        auto& gridVisSel = vimContext.getGridVisualSelection();
        if (gridVisSel.active)
        {
            int minTrack = std::min (gridVisSel.startTrack, gridVisSel.endTrack);
            int maxTrack = std::max (gridVisSel.startTrack, gridVisSel.endTrack);
            bool inRange = (static_cast<int> (i) >= minTrack && static_cast<int> (i) <= maxTrack);
            trackLanes[i]->setGridVisualSelection (gridVisSel.startPos, gridVisSel.endPos, inRange);
        }
        else
        {
            trackLanes[i]->setGridVisualSelection (0, 0, false);
        }
    }

    // Auto-scroll: keep grid cursor visible in viewport
    if (selectedTrack >= 0 && sr > 0.0)
    {
        float cursorScreenX = static_cast<float> ((static_cast<double> (gridPos) / sr) * pixelsPerSecond)
                            + 150.0f - scrollView.getScrollOffsetX();
        float viewWidth = scrollView.getWidth();

        if (cursorScreenX < 150.0f + 50.0f)
        {
            // Cursor is off the left edge — scroll left
            float targetScrollX = static_cast<float> ((static_cast<double> (gridPos) / sr) * pixelsPerSecond)
                                - 150.0f;
            scrollView.setScrollOffset (std::max (0.0f, targetScrollX), scrollView.getScrollOffsetY());
        }
        else if (cursorScreenX > viewWidth - 50.0f)
        {
            // Cursor is off the right edge — scroll right
            float targetScrollX = static_cast<float> ((static_cast<double> (gridPos) / sr) * pixelsPerSecond)
                                + 150.0f - viewWidth + 150.0f;
            scrollView.setScrollOffset (std::max (0.0f, targetScrollX), scrollView.getScrollOffsetY());
        }
    }
}

void ArrangementWidget::setActiveContext (bool active)
{
    if (activeContext != active)
    {
        activeContext = active;
        repaint();
    }
}

// ─── VimEngine::Listener ─────────────────────────────────────

void ArrangementWidget::vimModeChanged (VimEngine::Mode)
{
    updateSelectionVisuals();
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
    needsRebuild = true;
}

void ArrangementWidget::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    needsRebuild = true;
}

void ArrangementWidget::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    needsRebuild = true;
}

} // namespace ui
} // namespace dc
