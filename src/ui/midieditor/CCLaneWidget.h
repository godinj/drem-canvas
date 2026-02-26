#pragma once

#include "graphics/core/Widget.h"
#include "model/Project.h"
#include "model/MidiClip.h"
#include <JuceHeader.h>

namespace dc
{
namespace ui
{

class CCLaneWidget : public gfx::Widget
{
public:
    CCLaneWidget (Project& project);

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;
    void mouseUp (const gfx::MouseEvent& e) override;

    void setClipState (const juce::ValueTree& state) { clipState = state; repaint(); }
    void setPixelsPerBeat (float ppb) { pixelsPerBeat = ppb; repaint(); }
    void setScrollOffset (float offset) { scrollOffset = offset; repaint(); }
    void setCCNumber (int cc) { ccNumber = cc; repaint(); }
    int getCCNumber() const { return ccNumber; }

private:
    void addOrUpdateCCPoint (double beat, int value);

    Project& project;
    juce::ValueTree clipState;
    float pixelsPerBeat = 80.0f;
    float scrollOffset = 0.0f;
    int ccNumber = 1; // Default: modulation wheel

    bool drawing = false;
};

} // namespace ui
} // namespace dc
