#pragma once
#include <JuceHeader.h>
#include <filesystem>
#include <string>

namespace dc
{

class SessionWriter
{
public:
    /** Writes the project state to a session directory.
        Creates session.yaml + track-N.yaml files, and cleans up stale track files. */
    static bool writeSession (const juce::ValueTree& projectState, const std::filesystem::path& sessionDir);

private:
    static bool writeFileAtomically (const std::filesystem::path& targetFile, const std::string& content);
    static void cleanupStaleTrackFiles (const std::filesystem::path& sessionDir, int trackCount);
};

} // namespace dc
