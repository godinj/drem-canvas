#include "MixerPanel.h"

namespace dc
{

MixerPanel::MixerPanel (Project& proj, MixBusProcessor& bus)
    : project (proj),
      masterBus (bus)
{
    // Create master strip with a special "Master" ValueTree state
    juce::ValueTree masterState (IDs::TRACK);
    masterState.setProperty (IDs::name, "Master", nullptr);
    masterState.setProperty (IDs::volume, 1.0, nullptr);
    masterState.setProperty (IDs::pan, 0.0, nullptr);
    masterState.setProperty (IDs::mute, false, nullptr);
    masterState.setProperty (IDs::solo, false, nullptr);
    masterState.setProperty (IDs::colour, static_cast<int> (0xffff9020), nullptr);

    masterStrip = std::make_unique<ChannelStrip> (masterState);
    masterStrip->onStateChanged = [this]
    {
        // Sync master gain from the master strip fader
        // The fader range is 0..1.5, direct mapping to gain
        const float vol = static_cast<float> (masterStrip->getMeter().getParentComponent() != nullptr
                                                  ? masterBus.getMasterGain() : 1.0f);
        juce::ignoreUnused (vol);
    };
    addAndMakeVisible (*masterStrip);

    // Wire master meter to master bus peak levels
    masterMeter.getLeftLevel  = [this] { return masterBus.getPeakLevelLeft(); };
    masterMeter.getRightLevel = [this] { return masterBus.getPeakLevelRight(); };
    addAndMakeVisible (masterMeter);

    // Also wire the master strip's meter
    masterStrip->getMeter().getLeftLevel  = [this] { return masterBus.getPeakLevelLeft(); };
    masterStrip->getMeter().getRightLevel = [this] { return masterBus.getPeakLevelRight(); };

    // Listen to the TRACKS child of the project state for add/remove
    project.getState().getChildWithName (IDs::TRACKS).addListener (this);

    rebuildStrips();
}

MixerPanel::~MixerPanel()
{
    auto tracksTree = project.getState().getChildWithName (IDs::TRACKS);
    if (tracksTree.isValid())
        tracksTree.removeListener (this);
}

void MixerPanel::rebuildStrips()
{
    strips.clear();

    const int numTracks = project.getNumTracks();
    for (int i = 0; i < numTracks; ++i)
    {
        auto trackState = project.getTrack (i);
        auto* strip = strips.add (new ChannelStrip (trackState));
        addAndMakeVisible (strip);

        // Wire meter sources via callback if provided
        if (onWireMeter)
            onWireMeter (i, *strip);
    }

    resized();
    repaint();
}

void MixerPanel::setSelectedStripIndex (int index)
{
    if (selectedStripIndex == index)
        return;

    selectedStripIndex = index;

    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setSelected (i == index);

    if (masterStrip != nullptr)
        masterStrip->setSelected (index == strips.size());
}

void MixerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));

    // Draw separator line before master strip
    const int separatorX = static_cast<int> (strips.size()) * stripWidth + 8;
    g.setColour (juce::Colour (0xff555565));
    g.drawVerticalLine (separatorX, 0.0f, static_cast<float> (getHeight()));
}

void MixerPanel::resized()
{
    auto area = getLocalBounds();

    // Layout track strips left to right
    for (auto* strip : strips)
    {
        strip->setBounds (area.removeFromLeft (stripWidth));
    }

    // Gap before master section
    area.removeFromLeft (16);

    // Master strip
    if (masterStrip != nullptr)
        masterStrip->setBounds (area.removeFromLeft (stripWidth));

    // Master meter next to master strip
    masterMeter.setBounds (area.removeFromLeft (40).reduced (4, 8));
}

void MixerPanel::valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree& /*childWhichHasBeenAdded*/)
{
    if (parentTree.getType() == IDs::TRACKS)
        rebuildStrips();
}

void MixerPanel::valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree& /*childWhichHasBeenRemoved*/, int /*indexFromWhichChildWasRemoved*/)
{
    if (parentTree.getType() == IDs::TRACKS)
        rebuildStrips();
}

} // namespace dc
