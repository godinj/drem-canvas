#pragma once

#include "graphics/core/Widget.h"

namespace dc
{
namespace ui
{

class CpuMeterWidget : public gfx::Widget
{
public:
    CpuMeterWidget();

    void paint (gfx::Canvas& canvas) override;
    void animationTick (double timestampMs) override;

    /** Set the raw CPU load ratio (0.0–1.0) from the audio engine. */
    void setCpuLoad (float load);

private:
    float rawLoad = 0.0f;
    float smoothedLoad = 0.0f;
    float peakHold = 0.0f;
    double peakHoldTimer = 0.0;
};

} // namespace ui
} // namespace dc
