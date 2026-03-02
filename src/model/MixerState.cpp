#include "MixerState.h"

namespace dc
{

namespace
{
    const dc::PropertyId masterVolumeId ("masterVolume");
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
    return static_cast<float> (project.getState().getProperty (masterVolumeId).getDoubleOr (1.0));
}

void MixerState::setMasterVolume (float vol, UndoManager* um)
{
    project.getState().setProperty (masterVolumeId, Variant (static_cast<double> (vol)), um);
}

} // namespace dc
