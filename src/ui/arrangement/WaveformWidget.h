#pragma once

#include "graphics/core/Widget.h"
#include "graphics/rendering/WaveformCache.h"

namespace dc
{
namespace ui
{

class WaveformWidget : public gfx::Widget
{
public:
    WaveformWidget();

    void paint (gfx::Canvas& canvas) override;

    void setWaveformCache (gfx::WaveformCache* cache) { waveformCache = cache; repaint(); }
    void setPixelsPerSecond (double pps) { pixelsPerSecond = pps; repaint(); }
    void setSampleRate (double sr) { sampleRate = sr; repaint(); }

private:
    gfx::WaveformCache* waveformCache = nullptr;
    double pixelsPerSecond = 100.0;
    double sampleRate = 44100.0;
};

} // namespace ui
} // namespace dc
