#pragma once
#include <JuceHeader.h>
#include <filesystem>

namespace dc
{

class SessionReader
{
public:
    /** Reads a session directory and returns a complete PROJECT ValueTree.
        Returns an invalid ValueTree on failure. */
    static juce::ValueTree readSession (const std::filesystem::path& sessionDir);

    /** Checks whether the given directory contains a valid session.yaml file. */
    static bool isValidSessionDirectory (const std::filesystem::path& dir);
};

} // namespace dc
