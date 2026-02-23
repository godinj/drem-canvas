#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ScrollViewWidget.h"
#include "TimeRulerWidget.h"
#include "TrackLaneWidget.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "engine/TransportController.h"
#include <vector>
#include <memory>

namespace dc
{
namespace ui
{

class ArrangementWidget : public gfx::Widget,
                          public VimEngine::Listener
{
public:
    ArrangementWidget (Project& project, TransportController& transport,
                       Arrangement& arrangement, VimContext& vimContext);
    ~ArrangementWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void resized() override;
    void animationTick (double timestampMs) override;

    void rebuildTrackLanes();

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

    // ValueTree::Listener
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}

private:
    void updateSelectionVisuals();

    Project& project;
    TransportController& transportController;
    Arrangement& arrangement;
    VimContext& vimContext;

    TimeRulerWidget timeRuler;
    gfx::ScrollViewWidget scrollView;
    gfx::Widget trackContainer;

    std::vector<std::unique_ptr<TrackLaneWidget>> trackLanes;

    double pixelsPerSecond = 100.0;
    static constexpr float rulerHeight = 30.0f;
    static constexpr float trackHeight = 100.0f;
};

} // namespace ui
} // namespace dc
