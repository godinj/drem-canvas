#pragma once
#include <JuceHeader.h>

namespace dc
{

class SessionReader
{
public:
    /** Reads a session directory and returns a complete PROJECT ValueTree.
        Returns an invalid ValueTree on failure. */
    static juce::ValueTree readSession (const juce::File& sessionDir);

    /** Checks whether the given directory contains a valid session.yaml file. */
    static bool isValidSessionDirectory (const juce::File& dir);
};

} // namespace dc
