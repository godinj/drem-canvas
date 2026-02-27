#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include "graphics/rendering/WaveformCache.h"
#include "WaveformWidget.h"
#include "MidiClipWidget.h"
#include "vim/VimContext.h"
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
    void setTempo (double bpm);
    void setSelected (bool sel);
    void setSelectedClipIndex (int idx);
    void setVisualSelection (const VimContext::VisualSelection& sel, int trackIndex);
    void setGridCursorPosition (int64_t pos);
    void setGridUnitInSamples (int64_t unit);
    void setGridVisualSelection (int64_t startPos, int64_t endPos, bool active);

    bool isSelected() const { return selected; }
    const juce::ValueTree& getTrackState() const { return trackState; }

private:
    void rebuildClipViews();

    juce::ValueTree trackState;
    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;
    double tempo = 120.0;
    bool selected = false;
    int selectedClipIndex = -1;
    bool inVisualSelection = false;
    bool visualLinewise = false;
    int visualStartClip = -1;
    int visualEndClip = -1;

    int64_t gridCursorPosition = -1;
    int64_t gridUnitInSamples = 0;
    bool gridVisualActive = false;
    int64_t gridVisualStartPos = 0;
    int64_t gridVisualEndPos = 0;

    static constexpr float headerWidth = 150.0f;

    juce::AudioFormatManager formatManager;
    std::vector<std::unique_ptr<gfx::WaveformCache>> waveformCaches;
    std::vector<std::unique_ptr<gfx::Widget>> clipViews;
};

} // namespace ui
} // namespace dc
