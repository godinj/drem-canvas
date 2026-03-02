#include "SessionWriter.h"
#include "YAMLSerializer.h"
#include "model/Project.h"
#include "dc/foundation/file_utils.h"
#include <yaml-cpp/yaml.h>

namespace dc
{

bool SessionWriter::writeSession (const juce::ValueTree& projectState, const std::filesystem::path& sessionDir)
{
    std::error_code ec;
    std::filesystem::create_directories (sessionDir, ec);
    if (ec)
        return false;

    auto tracks = projectState.getChildWithName (IDs::TRACKS);
    int trackCount = tracks.getNumChildren();

    // Write session.yaml
    {
        auto metaNode = YAMLSerializer::emitSessionMeta (projectState, trackCount);
        YAML::Emitter emitter;
        emitter << metaNode;

        if (! writeFileAtomically (sessionDir / "session.yaml",
                                    std::string (emitter.c_str())))
            return false;
    }

    // Write track-N.yaml for each track
    for (int i = 0; i < trackCount; ++i)
    {
        auto trackNode = YAMLSerializer::emitTrack (tracks.getChild (i), sessionDir);
        YAML::Emitter emitter;
        emitter << trackNode;

        auto filename = "track-" + std::to_string (i) + ".yaml";
        if (! writeFileAtomically (sessionDir / filename,
                                    std::string (emitter.c_str())))
            return false;
    }

    cleanupStaleTrackFiles (sessionDir, trackCount);

    // Write sequencer.yaml if STEP_SEQUENCER child exists
    auto sequencerState = projectState.getChildWithName (IDs::STEP_SEQUENCER);
    if (sequencerState.isValid())
    {
        auto seqNode = YAMLSerializer::emitStepSequencer (sequencerState);
        YAML::Emitter emitter;
        emitter << seqNode;

        if (! writeFileAtomically (sessionDir / "sequencer.yaml",
                                    std::string (emitter.c_str())))
            return false;
    }

    // Write .gitignore if it doesn't already exist (preserve user customizations)
    auto gitignore = sessionDir / ".gitignore";
    if (! std::filesystem::exists (gitignore))
        dc::writeStringToFile (gitignore, "peaks/\nexport/\n*.tmp\n");

    return true;
}

bool SessionWriter::writeFileAtomically (const std::filesystem::path& targetFile, const std::string& content)
{
    return dc::writeStringToFile (targetFile, content);
}

void SessionWriter::cleanupStaleTrackFiles (const std::filesystem::path& sessionDir, int trackCount)
{
    // Remove any track-N.yaml files where N >= trackCount
    for (int i = trackCount; ; ++i)
    {
        auto file = sessionDir / ("track-" + std::to_string (i) + ".yaml");
        if (! std::filesystem::exists (file))
            break;
        std::filesystem::remove (file);
    }
}

} // namespace dc
