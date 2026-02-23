#include "SessionReader.h"
#include "YAMLSerializer.h"
#include "model/Project.h"
#include <yaml-cpp/yaml.h>

namespace dc
{

bool SessionReader::isValidSessionDirectory (const juce::File& dir)
{
    return dir.isDirectory() && dir.getChildFile ("session.yaml").existsAsFile();
}

juce::ValueTree SessionReader::readSession (const juce::File& sessionDir)
{
    if (! isValidSessionDirectory (sessionDir))
        return {};

    try
    {
        // Parse session.yaml
        auto sessionFile = sessionDir.getChildFile ("session.yaml");
        auto sessionNode = YAML::LoadFile (sessionFile.getFullPathName().toStdString());

        auto projectState = YAMLSerializer::parseSessionMeta (sessionNode);
        if (! projectState.isValid())
            return {};

        int trackCount = sessionNode["track_count"] ? sessionNode["track_count"].as<int>() : 0;

        // Parse each track-N.yaml
        auto tracks = projectState.getChildWithName (IDs::TRACKS);

        for (int i = 0; i < trackCount; ++i)
        {
            auto filename = juce::String ("track-") + juce::String (i) + ".yaml";
            auto trackFile = sessionDir.getChildFile (filename);

            if (! trackFile.existsAsFile())
                continue;

            auto trackNode = YAML::LoadFile (trackFile.getFullPathName().toStdString());
            auto trackState = YAMLSerializer::parseTrack (trackNode, sessionDir);

            if (trackState.isValid())
                tracks.appendChild (trackState, nullptr);
        }

        // Parse sequencer.yaml if it exists
        auto sequencerFile = sessionDir.getChildFile ("sequencer.yaml");
        if (sequencerFile.existsAsFile())
        {
            auto seqNode = YAML::LoadFile (sequencerFile.getFullPathName().toStdString());
            auto seqState = YAMLSerializer::parseStepSequencer (seqNode);
            if (seqState.isValid())
                projectState.appendChild (seqState, nullptr);
        }

        return projectState;
    }
    catch (const YAML::Exception&)
    {
        return {};
    }
}

} // namespace dc
