#pragma once
#include "dc/model/PropertyTree.h"
#include <filesystem>

namespace dc
{

class SessionReader
{
public:
    /** Reads a session directory and returns a complete PROJECT PropertyTree.
        Returns an invalid PropertyTree on failure. */
    static PropertyTree readSession (const std::filesystem::path& sessionDir);

    /** Checks whether the given directory contains a valid session.yaml file. */
    static bool isValidSessionDirectory (const std::filesystem::path& dir);
};

} // namespace dc
