#include "WaveformView.h"
#include "gui/common/ColourBridge.h"
#include <algorithm>
#include <cmath>

using dc::bridge::toJuce;

namespace dc
{

WaveformView::WaveformView()
{
}

WaveformView::~WaveformView()
{
}

void WaveformView::setFile (const std::filesystem::path& file)
{
    waveformCache.loadFromFile (file);
    repaint();
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (toJuce (waveformColour.darker (0.8f)));
    g.fillRect (bounds);

    if (! waveformCache.isLoaded() || waveformCache.getTotalSamples() == 0)
    {
        g.setColour (toJuce (dc::Colours::grey));
        g.setFont (juce::Font (12.0f));
        g.drawText ("No audio", bounds, juce::Justification::centred, false);
        return;
    }

    // Calculate pixels per second from the view width and total file duration
    double totalSeconds = static_cast<double> (waveformCache.getTotalSamples()) / sampleRate;
    double pixelsPerSecond = (totalSeconds > 0.0)
                                 ? static_cast<double> (bounds.getWidth()) / totalSeconds
                                 : 100.0;

    const auto* lod = waveformCache.getLOD (pixelsPerSecond, sampleRate);
    if (lod == nullptr || lod->data.empty())
    {
        g.setColour (toJuce (dc::Colours::grey));
        g.setFont (juce::Font (12.0f));
        g.drawText ("No audio", bounds, juce::Justification::centred, false);
        return;
    }

    // Draw waveform from LOD min/max pairs
    g.setColour (toJuce (waveformColour));

    float centreY = static_cast<float> (bounds.getCentreY());
    float halfHeight = static_cast<float> (bounds.getHeight()) * 0.5f;
    int numBuckets = static_cast<int> (lod->data.size());
    float bucketWidth = static_cast<float> (bounds.getWidth()) / static_cast<float> (numBuckets);

    for (int i = 0; i < numBuckets; ++i)
    {
        float x = static_cast<float> (bounds.getX()) + static_cast<float> (i) * bucketWidth;
        float minY = centreY - lod->data[static_cast<size_t> (i)].maxVal * halfHeight;
        float maxY = centreY - lod->data[static_cast<size_t> (i)].minVal * halfHeight;

        g.drawVerticalLine (static_cast<int> (x), minY, maxY);
    }
}

} // namespace dc
