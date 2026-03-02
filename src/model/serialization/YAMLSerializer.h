#pragma once
#include "dc/model/PropertyTree.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <filesystem>
#include "dc/foundation/types.h"

namespace dc
{

class YAMLSerializer
{
public:
    // Emit session metadata as YAML
    static YAML::Node emitSessionMeta (const PropertyTree& projectState, int trackCount);

    // Emit a single track as YAML
    static YAML::Node emitTrack (const PropertyTree& trackState, const std::filesystem::path& sessionDir);

    // Parse session metadata YAML into a PROJECT PropertyTree (without TRACKS children)
    static PropertyTree parseSessionMeta (const YAML::Node& node);

    // Parse a single track YAML into a TRACK PropertyTree
    static PropertyTree parseTrack (const YAML::Node& node, const std::filesystem::path& sessionDir);

    // Emit step sequencer as YAML
    static YAML::Node emitStepSequencer (const PropertyTree& sequencerState);

    // Parse step sequencer YAML into a STEP_SEQUENCER PropertyTree
    static PropertyTree parseStepSequencer (const YAML::Node& node);

    // Emit/parse plugin chain
    static YAML::Node emitPluginChain (const PropertyTree& trackState);
    static void parsePluginChain (const YAML::Node& pluginsNode, PropertyTree& trackState);

private:
    static YAML::Node emitAudioClip (const PropertyTree& clipState, const std::filesystem::path& sessionDir);
    static YAML::Node emitMidiClip (const PropertyTree& clipState);

    static PropertyTree parseAudioClip (const YAML::Node& node, const std::filesystem::path& sessionDir);
    static PropertyTree parseMidiClip (const YAML::Node& node);

    static std::string colourToHex (dc::Colour c);
    static dc::Colour hexToColour (const std::string& hex);

    static std::string makeRelativePath (const std::filesystem::path& file, const std::filesystem::path& sessionDir);
    static std::filesystem::path resolveRelativePath (const std::string& relativePath, const std::filesystem::path& sessionDir);

    static YAML::Node emitStepPattern (const PropertyTree& patternState);
    static YAML::Node emitStepRow (const PropertyTree& rowState);
    static PropertyTree parseStepPattern (const YAML::Node& node);
    static PropertyTree parseStepRow (const YAML::Node& node);
};

} // namespace dc
