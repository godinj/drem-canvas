#include "WaveformWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

WaveformWidget::WaveformWidget()
{
    useTextureCache = true;
}

void WaveformWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();

    Rect r (0, 0, getWidth(), getHeight());

    // Background
    canvas.fillRect (r, Color::fromARGB (0xff222238));

    if (!waveformCache || !waveformCache->isLoaded())
        return;

    // Get appropriate LOD data
    const auto* lod = waveformCache->getLOD (pixelsPerSecond, sampleRate);
    if (!lod || lod->data.empty())
        return;

    // Build sample array for canvas drawWaveform
    int numPixels = static_cast<int> (getWidth());
    std::vector<Canvas::WaveformSample> samples (static_cast<size_t> (numPixels));

    int64_t totalSamples = waveformCache->getTotalSamples();
    double samplesPerPixel = (totalSamples > 0) ? static_cast<double> (totalSamples) / static_cast<double> (numPixels) : 1.0;
    int spb = lod->samplesPerBucket;

    for (int px = 0; px < numPixels; ++px)
    {
        int64_t sampleStart = static_cast<int64_t> (px * samplesPerPixel);
        int64_t sampleEnd = static_cast<int64_t> ((px + 1) * samplesPerPixel);
        int bucketStart = static_cast<int> (sampleStart / spb);
        int bucketEnd = static_cast<int> (sampleEnd / spb);

        float minVal = 0.0f;
        float maxVal = 0.0f;

        for (int b = bucketStart; b <= bucketEnd && b < static_cast<int> (lod->data.size()); ++b)
        {
            if (b >= 0)
            {
                minVal = std::min (minVal, lod->data[static_cast<size_t> (b)].minVal);
                maxVal = std::max (maxVal, lod->data[static_cast<size_t> (b)].maxVal);
            }
        }

        samples[static_cast<size_t> (px)] = { minVal, maxVal };
    }

    canvas.drawWaveform (r, samples, theme.waveformFill);
}

} // namespace ui
} // namespace dc
