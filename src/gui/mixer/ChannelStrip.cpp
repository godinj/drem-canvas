#include "ChannelStrip.h"

namespace dc
{

ChannelStrip::ChannelStrip (const juce::ValueTree& state, UndoSystem* us)
    : trackState (state),
      undoSystem (us),
      pluginSlotList (state)
{
    trackState.addListener (this);

    // Fader setup - vertical slider for volume
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setRange (0.0, 1.5, 0.01);
    fader.setSkewFactorFromMidPoint (0.5);
    fader.setValue (trackState.getProperty (IDs::volume, 1.0), juce::dontSendNotification);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.onValueChange = [this]
    {
        juce::UndoManager* um = nullptr;
        if (undoSystem != nullptr)
        {
            undoSystem->beginCoalescedTransaction ("Adjust Volume");
            um = &undoSystem->getUndoManager();
        }
        trackState.setProperty (IDs::volume, static_cast<float> (fader.getValue()), um);
        if (onStateChanged)
            onStateChanged();
    };
    addAndMakeVisible (fader);

    // Pan knob setup - rotary knob
    panKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panKnob.setRange (-1.0, 1.0, 0.01);
    panKnob.setValue (trackState.getProperty (IDs::pan, 0.0), juce::dontSendNotification);
    panKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panKnob.onValueChange = [this]
    {
        juce::UndoManager* um = nullptr;
        if (undoSystem != nullptr)
        {
            undoSystem->beginCoalescedTransaction ("Adjust Pan");
            um = &undoSystem->getUndoManager();
        }
        trackState.setProperty (IDs::pan, static_cast<float> (panKnob.getValue()), um);
        if (onStateChanged)
            onStateChanged();
    };
    addAndMakeVisible (panKnob);

    // Mute button
    muteButton.setClickingTogglesState (true);
    muteButton.setToggleable (true);
    muteButton.setToggleState (trackState.getProperty (IDs::mute, false), juce::dontSendNotification);
    muteButton.onClick = [this]
    {
        juce::UndoManager* um = nullptr;
        if (undoSystem != nullptr)
        {
            undoSystem->beginTransaction ("Toggle Mute");
            um = &undoSystem->getUndoManager();
        }
        trackState.setProperty (IDs::mute, muteButton.getToggleState(), um);
        if (onStateChanged)
            onStateChanged();
    };
    addAndMakeVisible (muteButton);

    // Solo button
    soloButton.setClickingTogglesState (true);
    soloButton.setToggleable (true);
    soloButton.setToggleState (trackState.getProperty (IDs::solo, false), juce::dontSendNotification);
    soloButton.onClick = [this]
    {
        juce::UndoManager* um = nullptr;
        if (undoSystem != nullptr)
        {
            undoSystem->beginTransaction ("Toggle Solo");
            um = &undoSystem->getUndoManager();
        }
        trackState.setProperty (IDs::solo, soloButton.getToggleState(), um);
        if (onStateChanged)
            onStateChanged();
    };
    addAndMakeVisible (soloButton);

    // Name label
    nameLabel.setText (trackState.getProperty (IDs::name, "Track").toString(),
                       juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
    nameLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (nameLabel);

    // Meter
    addAndMakeVisible (meter);

    // Plugin slot list
    pluginSlotList.onPluginClicked = [this] (int idx)
    {
        if (onPluginClicked)
            onPluginClicked (idx);
    };
    pluginSlotList.onPluginBypassToggled = [this] (int idx)
    {
        if (onPluginBypassToggled)
            onPluginBypassToggled (idx);
    };
    pluginSlotList.onPluginRemoveRequested = [this] (int idx)
    {
        if (onPluginRemoveRequested)
            onPluginRemoveRequested (idx);
    };
    addAndMakeVisible (pluginSlotList);
}

ChannelStrip::~ChannelStrip()
{
    trackState.removeListener (this);
}

void ChannelStrip::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void ChannelStrip::paint (juce::Graphics& g)
{
    // Background fill — slightly brighter when selected
    g.fillAll (selected ? juce::Colour (0xff33334a) : juce::Colour (0xff2a2a3a));

    // Top colour bar from track colour
    const auto colourValue = trackState.getProperty (IDs::colour, static_cast<int> (0xff4a9eff));
    const auto trackColour = juce::Colour (static_cast<juce::uint32> (static_cast<int> (colourValue)));
    g.setColour (trackColour);
    g.fillRect (0, 0, getWidth(), 4);

    // Selection border
    if (selected)
    {
        g.setColour (juce::Colour (0xff50c878));
        g.drawRect (getLocalBounds(), 2);
    }
}

void ChannelStrip::paintOverChildren (juce::Graphics& g)
{
    if (! selected || currentFocus == VimContext::FocusNone)
        return;

    juce::Component* focusedComponent = nullptr;

    switch (currentFocus)
    {
        case VimContext::FocusVolume:  focusedComponent = &fader; break;
        case VimContext::FocusPan:     focusedComponent = &panKnob; break;
        case VimContext::FocusPlugins: focusedComponent = &pluginSlotList; break;
        default: break;
    }

    if (focusedComponent != nullptr)
    {
        auto focusBounds = focusedComponent->getBounds().toFloat().expanded (2.0f);

        // Subtle green fill
        g.setColour (juce::Colour (0xff50c878).withAlpha (0.08f));
        g.fillRoundedRectangle (focusBounds, 2.0f);

        // Green stroke
        g.setColour (juce::Colour (0xff50c878).withAlpha (0.6f));
        g.drawRoundedRectangle (focusBounds, 2.0f, 1.5f);
    }
}

void ChannelStrip::setMixerFocus (VimContext::MixerFocus focus)
{
    if (currentFocus != focus)
    {
        currentFocus = focus;
        repaint();
    }
}

void ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced (2);

    // Top colour bar space
    area.removeFromTop (6);

    // Name label at top
    nameLabel.setBounds (area.removeFromTop (20));

    // Plugin slot list
    pluginSlotList.setBounds (area.removeFromTop (PluginSlotList::preferredHeight));

    // Meter takes the main middle space
    auto meterArea = area.removeFromTop (area.getHeight() / 2);
    meter.setBounds (meterArea.reduced (4, 2));

    // Pan knob
    const int knobSize = std::min (area.getWidth(), 40);
    auto panArea = area.removeFromTop (knobSize);
    panKnob.setBounds (panArea.withSizeKeepingCentre (knobSize, knobSize));

    // Mute / Solo buttons row
    auto buttonRow = area.removeFromTop (24);
    const int buttonWidth = buttonRow.getWidth() / 2;
    muteButton.setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (1));
    soloButton.setBounds (buttonRow.reduced (1));

    // Fader takes remaining space at bottom
    fader.setBounds (area.reduced (4, 2));
}

void ChannelStrip::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree != trackState)
        return;

    // Sync UI controls from model when properties change externally
    if (property == IDs::volume)
        fader.setValue (static_cast<double> (tree.getProperty (IDs::volume)),
                        juce::dontSendNotification);
    else if (property == IDs::pan)
        panKnob.setValue (static_cast<double> (tree.getProperty (IDs::pan)),
                          juce::dontSendNotification);
    else if (property == IDs::mute)
        muteButton.setToggleState (tree.getProperty (IDs::mute),
                                    juce::dontSendNotification);
    else if (property == IDs::solo)
        soloButton.setToggleState (tree.getProperty (IDs::solo),
                                    juce::dontSendNotification);
    else if (property == IDs::name)
        nameLabel.setText (tree.getProperty (IDs::name).toString(),
                           juce::dontSendNotification);
}

} // namespace dc
