#include "SessionReader.h"
#include "YAMLSerializer.h"
#include "model/Project.h"
#include <yaml-cpp/yaml.h>

namespace dc
{

bool SessionReader::isValidSessionDirectory (const std::filesystem::path& dir)
{
    return std::filesystem::is_directory (dir) && std::filesystem::exists (dir / "session.yaml");
}

PropertyTree SessionReader::readSession (const std::filesystem::path& sessionDir)
{
    if (! isValidSessionDirectory (sessionDir))
        return {};

    try
    {
        // Parse session.yaml
        auto sessionNode = YAML::LoadFile ((sessionDir / "session.yaml").string());

        auto projectState = YAMLSerializer::parseSessionMeta (sessionNode);
        if (! projectState.isValid())
            return {};

        int trackCount = sessionNode["track_count"] ? sessionNode["track_count"].as<int>() : 0;

        // Parse each track-N.yaml
        auto tracks = projectState.getChildWithType (IDs::TRACKS);

        for (int i = 0; i < trackCount; ++i)
        {
            auto trackFile = sessionDir / ("track-" + std::to_string (i) + ".yaml");

            if (! std::filesystem::exists (trackFile))
                continue;

            auto trackNode = YAML::LoadFile (trackFile.string());
            auto trackState = YAMLSerializer::parseTrack (trackNode, sessionDir);

            if (trackState.isValid())
                tracks.addChild (trackState, -1);
        }

        // Parse sequencer.yaml if it exists
        auto sequencerFile = sessionDir / "sequencer.yaml";
        if (std::filesystem::exists (sequencerFile))
        {
            auto seqNode = YAML::LoadFile (sequencerFile.string());
            auto seqState = YAMLSerializer::parseStepSequencer (seqNode);
            if (seqState.isValid())
                projectState.addChild (seqState, -1);
        }

        return projectState;
    }
    catch (const YAML::Exception&)
    {
        return {};
    }
}

} // namespace dc
