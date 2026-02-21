#pragma once
#include <JuceHeader.h>

namespace dc
{

class SessionWriter
{
public:
    /** Writes the project state to a session directory.
        Creates session.yaml + track-N.yaml files, and cleans up stale track files. */
    static bool writeSession (const juce::ValueTree& projectState, const juce::File& sessionDir);

private:
    static bool writeFileAtomically (const juce::File& targetFile, const juce::String& content);
    static void cleanupStaleTrackFiles (const juce::File& sessionDir, int trackCount);
};

} // namespace dc
