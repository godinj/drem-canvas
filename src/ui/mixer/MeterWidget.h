#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <atomic>

namespace dc
{
namespace ui
{

class MeterWidget : public gfx::Widget
{
public:
    MeterWidget();

    void paint (gfx::Canvas& canvas) override;
    void animationTick (double timestampMs) override;

    void setLevel (float leftDb, float rightDb);
    void setPeakHold (float leftPeak, float rightPeak);

private:
    float leftLevel = -60.0f;
    float rightLevel = -60.0f;
    float leftPeak = -60.0f;
    float rightPeak = -60.0f;

    // Smoothed display values
    float displayLeft = 0.0f;
    float displayRight = 0.0f;
    float peakHoldLeft = 0.0f;
    float peakHoldRight = 0.0f;
    double peakHoldTimerLeft = 0.0;
    double peakHoldTimerRight = 0.0;
};

} // namespace ui
} // namespace dc
