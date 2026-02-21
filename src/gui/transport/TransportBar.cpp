#include "TransportBar.h"

namespace dc
{

TransportBar::TransportBar (TransportController& transport)
    : transportController (transport)
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (openButton);
    addAndMakeVisible (timeDisplay);

    playButton.onClick = [this]
    {
        transportController.togglePlayStop();
    };

    stopButton.onClick = [this]
    {
        transportController.stop();
        transportController.setPositionInSamples (0);
    };

    openButton.onClick = [this]
    {
        if (onOpenFile)
            onOpenFile();
    };

    timeDisplay.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));
    timeDisplay.setJustificationType (juce::Justification::centred);
    timeDisplay.setColour (juce::Label::textColourId, juce::Colours::white);
    timeDisplay.setText ("00:00.000", juce::dontSendNotification);

    startTimerHz (30);
}

void TransportBar::timerCallback()
{
    timeDisplay.setText (transportController.getTimeString(), juce::dontSendNotification);

    if (transportController.isPlaying())
        playButton.setButtonText ("Pause");
    else
        playButton.setButtonText ("Play");
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2d2d3d));
}

void TransportBar::resized()
{
    auto bounds = getLocalBounds();
    int buttonWidth = 70;
    int margin = 4;

    // Play and Stop buttons on the left
    playButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (margin));
    stopButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (margin));

    // Open button on the right
    openButton.setBounds (bounds.removeFromRight (100).reduced (margin));

    // Time display in the remaining center area
    timeDisplay.setBounds (bounds.reduced (margin));
}

} // namespace dc
