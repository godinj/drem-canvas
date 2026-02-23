#include "Arrangement.h"
#include "utils/UndoSystem.h"

namespace dc
{

Arrangement::Arrangement (Project& p)
    : project (p)
{
}

int Arrangement::getNumTracks() const
{
    return project.getNumTracks();
}

Track Arrangement::getTrack (int index) const
{
    return Track (project.getTrack (index));
}

Track Arrangement::addTrack (const juce::String& name)
{
    auto trackState = project.addTrack (name);
    return Track (trackState);
}

void Arrangement::removeTrack (int index)
{
    if (selectedTrackIndex == index)
        selectedTrackIndex = -1;
    else if (selectedTrackIndex > index)
        --selectedTrackIndex;

    project.removeTrack (index);
}

void Arrangement::moveTrack (int fromIndex, int toIndex)
{
    auto& state = project.getState();
    auto tracksNode = state.getChildWithName (IDs::TRACKS);

    if (! tracksNode.isValid())
        return;

    if (fromIndex < 0 || fromIndex >= tracksNode.getNumChildren())
        return;

    if (toIndex < 0 || toIndex >= tracksNode.getNumChildren())
        return;

    if (fromIndex == toIndex)
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Move Track");
    auto trackToMove = tracksNode.getChild (fromIndex);
    tracksNode.removeChild (trackToMove, &project.getUndoManager());
    tracksNode.addChild (trackToMove, toIndex, &project.getUndoManager());

    // Update selection to follow the moved track
    if (selectedTrackIndex == fromIndex)
        selectedTrackIndex = toIndex;
    else if (fromIndex < selectedTrackIndex && toIndex >= selectedTrackIndex)
        --selectedTrackIndex;
    else if (fromIndex > selectedTrackIndex && toIndex <= selectedTrackIndex)
        ++selectedTrackIndex;
}

void Arrangement::selectTrack (int index)
{
    if (index >= 0 && index < getNumTracks())
        selectedTrackIndex = index;
}

void Arrangement::deselectAll()
{
    selectedTrackIndex = -1;
}

bool Arrangement::isTrackAudible (int index) const
{
    if (index < 0 || index >= getNumTracks())
        return false;

    // Check if any track is solo'd
    bool anySolo = false;

    for (int i = 0; i < getNumTracks(); ++i)
    {
        Track t (project.getTrack (i));

        if (t.isSolo())
        {
            anySolo = true;
            break;
        }
    }

    Track track (project.getTrack (index));

    if (anySolo)
        return track.isSolo();

    return ! track.isMuted();
}

} // namespace dc
