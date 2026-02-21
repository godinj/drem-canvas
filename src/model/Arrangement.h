#pragma once
#include <JuceHeader.h>
#include "Project.h"
#include "Track.h"

namespace dc
{

class Arrangement
{
public:
    explicit Arrangement (Project& project);

    int getNumTracks() const;
    Track getTrack (int index) const;

    Track addTrack (const juce::String& name);
    void removeTrack (int index);
    void moveTrack (int fromIndex, int toIndex);

    // Selection
    void selectTrack (int index);
    void deselectAll();
    int getSelectedTrackIndex() const { return selectedTrackIndex; }

    // Solo logic - returns true if a given track should be audible
    bool isTrackAudible (int index) const;

private:
    Project& project;
    int selectedTrackIndex = -1;
};

} // namespace dc
