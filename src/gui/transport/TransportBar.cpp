#include "TransportBar.h"

namespace dc
{

TransportBar::TransportBar (TransportController& transport, Project& proj, TempoMap& tempo)
    : transportController (transport),
      project (proj),
      tempoMap (tempo)
{
    // Transport buttons
    addAndMakeVisible (rtzButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (recordButton);

    rtzButton.onClick = [this] { transportController.returnToZero(); };

    stopButton.onClick = [this] { transportController.stop(); };

    playButton.onClick = [this] { transportController.togglePlayStop(); };

    recordButton.onClick = [this]
    {
        transportController.toggleRecordArm();
    };

    // Time display — click to toggle format
    addAndMakeVisible (timeDisplay);
    timeDisplay.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));
    timeDisplay.setJustificationType (juce::Justification::centred);
    timeDisplay.setColour (juce::Label::textColourId, juce::Colours::white);
    timeDisplay.setText ("00:00.000", juce::dontSendNotification);
    timeDisplay.setInterceptsMouseClicks (true, false);
    timeDisplay.onClick = [this]
    {
        showBarsBeatsTicks = ! showBarsBeatsTicks;
    };

    // Tempo display — editable on double-click
    addAndMakeVisible (tempoDisplay);
    tempoDisplay.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    tempoDisplay.setJustificationType (juce::Justification::centred);
    tempoDisplay.setColour (juce::Label::textColourId, juce::Colours::white);
    tempoDisplay.setEditable (false, true); // single-click: no, double-click: yes
    tempoDisplay.setText (juce::String (project.getTempo(), 1) + " BPM", juce::dontSendNotification);
    tempoDisplay.addListener (this);

    // Time signature display (read-only)
    addAndMakeVisible (timeSigDisplay);
    timeSigDisplay.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    timeSigDisplay.setJustificationType (juce::Justification::centred);
    timeSigDisplay.setColour (juce::Label::textColourId, juce::Colours::white);
    timeSigDisplay.setText (juce::String (project.getTimeSigNumerator()) + "/"
                            + juce::String (project.getTimeSigDenominator()),
                            juce::dontSendNotification);

    // Toggle buttons
    addAndMakeVisible (metronomeButton);
    metronomeButton.setClickingTogglesState (true);
    metronomeButton.onClick = [this]
    {
        bool toggled = metronomeButton.getToggleState();
        if (onMetronomeToggled)
            onMetronomeToggled (toggled);
    };

    addAndMakeVisible (loopButton);
    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this]
    {
        transportController.setLoopEnabled (loopButton.getToggleState());
    };

    startTimerHz (30);
}

void TransportBar::labelTextChanged (juce::Label* labelThatHasChanged)
{
    if (labelThatHasChanged == &tempoDisplay)
    {
        // Strip " BPM" suffix if user left it in
        auto text = tempoDisplay.getText().trimCharactersAtEnd (" BPMbpm");
        double newTempo = text.getDoubleValue();
        newTempo = juce::jlimit (20.0, 999.0, newTempo);
        project.setTempo (newTempo);
    }
}

void TransportBar::timerCallback()
{
    // Update time display
    if (showBarsBeatsTicks)
    {
        auto pos = tempoMap.samplesToBarBeat (transportController.getPositionInSamples(),
                                               transportController.getSampleRate());
        timeDisplay.setText (tempoMap.formatBarBeat (pos), juce::dontSendNotification);
    }
    else
    {
        timeDisplay.setText (transportController.getTimeString(), juce::dontSendNotification);
    }

    // Update play button text
    playButton.setButtonText (transportController.isPlaying() ? "Pause" : "Play");

    // Update record button colour
    bool armed = transportController.isRecordArmed();
    recordButton.setToggleState (armed, juce::dontSendNotification);
    recordButton.setColour (juce::TextButton::buttonColourId,
                            armed ? juce::Colour (0xffcc3333) : juce::Colour (0xff3d3d5c));

    // Update loop button state
    loopButton.setToggleState (transportController.isLooping(), juce::dontSendNotification);

    // Update tempo display (if not being edited)
    if (! tempoDisplay.isBeingEdited())
        tempoDisplay.setText (juce::String (project.getTempo(), 1) + " BPM", juce::dontSendNotification);

    // Update time signature display
    timeSigDisplay.setText (juce::String (project.getTimeSigNumerator()) + "/"
                            + juce::String (project.getTimeSigDenominator()),
                            juce::dontSendNotification);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2d2d3d));
}

void TransportBar::resized()
{
    auto bounds = getLocalBounds().reduced (2, 0);
    int margin = 2;

    // Transport buttons
    rtzButton.setBounds (bounds.removeFromLeft (36).reduced (margin));
    stopButton.setBounds (bounds.removeFromLeft (50).reduced (margin));
    playButton.setBounds (bounds.removeFromLeft (50).reduced (margin));
    recordButton.setBounds (bounds.removeFromLeft (36).reduced (margin));

    bounds.removeFromLeft (8); // spacer

    // Time display
    timeDisplay.setBounds (bounds.removeFromLeft (120).reduced (margin));

    bounds.removeFromLeft (8); // spacer

    // Tempo and time sig
    tempoDisplay.setBounds (bounds.removeFromLeft (80).reduced (margin));
    timeSigDisplay.setBounds (bounds.removeFromLeft (36).reduced (margin));

    bounds.removeFromLeft (8); // spacer

    // Toggle buttons
    metronomeButton.setBounds (bounds.removeFromLeft (50).reduced (margin));
    loopButton.setBounds (bounds.removeFromLeft (50).reduced (margin));
}

} // namespace dc
