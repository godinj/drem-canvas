#pragma once
#include <JuceHeader.h>
#include "Project.h"
#include "Track.h"

namespace dc
{

class MixerState
{
public:
    explicit MixerState (Project& project);

    int getNumChannels() const;
    Track getChannel (int index) const;

    // Master volume
    float getMasterVolume() const;
    void setMasterVolume (float vol, juce::UndoManager* um = nullptr);

private:
    Project& project;
};

} // namespace dc
