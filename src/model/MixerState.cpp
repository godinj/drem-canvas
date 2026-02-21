#include "MixerState.h"

namespace dc
{

namespace
{
    const juce::Identifier masterVolumeId ("masterVolume");
}

MixerState::MixerState (Project& p)
    : project (p)
{
}

int MixerState::getNumChannels() const
{
    return project.getNumTracks();
}

Track MixerState::getChannel (int index) const
{
    return Track (project.getTrack (index));
}

float MixerState::getMasterVolume() const
{
    return static_cast<float> (project.getState().getProperty (masterVolumeId, 1.0f));
}

void MixerState::setMasterVolume (float vol, juce::UndoManager* um)
{
    project.getState().setProperty (masterVolumeId, vol, um);
}

} // namespace dc
