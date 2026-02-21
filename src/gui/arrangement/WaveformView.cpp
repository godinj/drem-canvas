#include "WaveformView.h"

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

void WaveformView::setFile (const juce::File& file)
{
    thumbnail.setSource (new juce::FileInputSource (file));
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (waveformColour.darker (0.8f));
    g.fillRect (bounds);

    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour (waveformColour);
        thumbnail.drawChannels (g, bounds, 0.0, thumbnail.getTotalLength(), 1.0f);
    }
    else
    {
        g.setColour (juce::Colours::grey);
        g.setFont (juce::Font (12.0f));
        g.drawText ("No audio", bounds, juce::Justification::centred, false);
    }
}

void WaveformView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

} // namespace dc
