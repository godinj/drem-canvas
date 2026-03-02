#pragma once
#include <JuceHeader.h>
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
    static YAML::Node emitSessionMeta (const juce::ValueTree& projectState, int trackCount);

    // Emit a single track as YAML
    static YAML::Node emitTrack (const juce::ValueTree& trackState, const std::filesystem::path& sessionDir);

    // Parse session metadata YAML into a PROJECT ValueTree (without TRACKS children)
    static juce::ValueTree parseSessionMeta (const YAML::Node& node);

    // Parse a single track YAML into a TRACK ValueTree
    static juce::ValueTree parseTrack (const YAML::Node& node, const std::filesystem::path& sessionDir);

    // Emit step sequencer as YAML
    static YAML::Node emitStepSequencer (const juce::ValueTree& sequencerState);

    // Parse step sequencer YAML into a STEP_SEQUENCER ValueTree
    static juce::ValueTree parseStepSequencer (const YAML::Node& node);

    // Emit/parse plugin chain
    static YAML::Node emitPluginChain (const juce::ValueTree& trackState);
    static void parsePluginChain (const YAML::Node& pluginsNode, juce::ValueTree& trackState);

private:
    static YAML::Node emitAudioClip (const juce::ValueTree& clipState, const std::filesystem::path& sessionDir);
    static YAML::Node emitMidiClip (const juce::ValueTree& clipState);

    static juce::ValueTree parseAudioClip (const YAML::Node& node, const std::filesystem::path& sessionDir);
    static juce::ValueTree parseMidiClip (const YAML::Node& node);

    static std::string colourToHex (dc::Colour c);
    static dc::Colour hexToColour (const std::string& hex);

    static std::string makeRelativePath (const std::filesystem::path& file, const std::filesystem::path& sessionDir);
    static std::filesystem::path resolveRelativePath (const std::string& relativePath, const std::filesystem::path& sessionDir);

    static YAML::Node emitStepPattern (const juce::ValueTree& patternState);
    static YAML::Node emitStepRow (const juce::ValueTree& rowState);
    static juce::ValueTree parseStepPattern (const YAML::Node& node);
    static juce::ValueTree parseStepRow (const YAML::Node& node);
};

} // namespace dc
