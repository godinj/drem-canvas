#include "SessionWriter.h"
#include "YAMLSerializer.h"
#include "model/Project.h"
#include <yaml-cpp/yaml.h>

namespace dc
{

bool SessionWriter::writeSession (const juce::ValueTree& projectState, const juce::File& sessionDir)
{
    if (! sessionDir.createDirectory())
        return false;

    auto tracks = projectState.getChildWithName (IDs::TRACKS);
    int trackCount = tracks.getNumChildren();

    // Write session.yaml
    {
        auto metaNode = YAMLSerializer::emitSessionMeta (projectState, trackCount);
        YAML::Emitter emitter;
        emitter << metaNode;

        if (! writeFileAtomically (sessionDir.getChildFile ("session.yaml"),
                                    juce::String (emitter.c_str())))
            return false;
    }

    // Write track-N.yaml for each track
    for (int i = 0; i < trackCount; ++i)
    {
        auto trackNode = YAMLSerializer::emitTrack (tracks.getChild (i), sessionDir);
        YAML::Emitter emitter;
        emitter << trackNode;

        auto filename = juce::String ("track-") + juce::String (i) + ".yaml";
        if (! writeFileAtomically (sessionDir.getChildFile (filename),
                                    juce::String (emitter.c_str())))
            return false;
    }

    cleanupStaleTrackFiles (sessionDir, trackCount);

    // Write .gitignore if it doesn't already exist (preserve user customizations)
    auto gitignore = sessionDir.getChildFile (".gitignore");
    if (! gitignore.existsAsFile())
        gitignore.replaceWithText ("peaks/\nexport/\n*.tmp\n");

    return true;
}

bool SessionWriter::writeFileAtomically (const juce::File& targetFile, const juce::String& content)
{
    auto tmpFile = targetFile.getSiblingFile (targetFile.getFileName() + ".tmp");

    if (! tmpFile.replaceWithText (content))
        return false;

    return tmpFile.moveFileTo (targetFile);
}

void SessionWriter::cleanupStaleTrackFiles (const juce::File& sessionDir, int trackCount)
{
    // Remove any track-N.yaml files where N >= trackCount
    for (int i = trackCount; ; ++i)
    {
        auto file = sessionDir.getChildFile (juce::String ("track-") + juce::String (i) + ".yaml");
        if (! file.existsAsFile())
            break;
        file.deleteFile();
    }
}

} // namespace dc
