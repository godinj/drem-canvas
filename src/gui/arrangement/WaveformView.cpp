#include "WaveformView.h"
#include "gui/common/ColourBridge.h"

using dc::bridge::toJuce;

namespace dc
{

WaveformView::WaveformView()
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener (this);
}

WaveformView::~WaveformView()
{
    thumbnail.removeChangeListener (this);
}

void WaveformView::setFile (const std::filesystem::path& file)
{
    thumbnail.setSource (new juce::FileInputSource (juce::File (file.string())));
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (toJuce (waveformColour.darker (0.8f)));
    g.fillRect (bounds);

    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour (toJuce (waveformColour));
        thumbnail.drawChannels (g, bounds, 0.0, thumbnail.getTotalLength(), 1.0f);
    }
    else
    {
        g.setColour (toJuce (dc::Colours::grey));
        g.setFont (juce::Font (12.0f));
        g.drawText ("No audio", bounds, juce::Justification::centred, false);
    }
}

void WaveformView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

} // namespace dc
