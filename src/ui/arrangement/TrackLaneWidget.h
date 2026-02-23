#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include "WaveformWidget.h"
#include "MidiClipWidget.h"
#include <JuceHeader.h>
#include <vector>
#include <memory>

namespace dc
{
namespace ui
{

class TrackLaneWidget : public gfx::Widget
{
public:
    explicit TrackLaneWidget (const juce::ValueTree& trackState);

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void resized() override;

    void setPixelsPerSecond (double pps);
    void setSampleRate (double sr);
    void setSelected (bool sel);
    void setSelectedClipIndex (int idx);

    bool isSelected() const { return selected; }
    const juce::ValueTree& getTrackState() const { return trackState; }

private:
    void rebuildClipViews();

    juce::ValueTree trackState;
    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;
    bool selected = false;
    int selectedClipIndex = -1;

    static constexpr float headerWidth = 150.0f;

    std::vector<std::unique_ptr<WaveformWidget>> clipViews;
};

} // namespace ui
} // namespace dc
