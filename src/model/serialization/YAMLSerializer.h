#pragma once
#include <JuceHeader.h>
#include <yaml-cpp/yaml.h>

namespace dc
{

class YAMLSerializer
{
public:
    // Emit session metadata as YAML
    static YAML::Node emitSessionMeta (const juce::ValueTree& projectState, int trackCount);

    // Emit a single track as YAML
    static YAML::Node emitTrack (const juce::ValueTree& trackState, const juce::File& sessionDir);

    // Parse session metadata YAML into a PROJECT ValueTree (without TRACKS children)
    static juce::ValueTree parseSessionMeta (const YAML::Node& node);

    // Parse a single track YAML into a TRACK ValueTree
    static juce::ValueTree parseTrack (const YAML::Node& node, const juce::File& sessionDir);

    // Emit step sequencer as YAML
    static YAML::Node emitStepSequencer (const juce::ValueTree& sequencerState);

    // Parse step sequencer YAML into a STEP_SEQUENCER ValueTree
    static juce::ValueTree parseStepSequencer (const YAML::Node& node);

    // Emit/parse plugin chain
    static YAML::Node emitPluginChain (const juce::ValueTree& trackState);
    static void parsePluginChain (const YAML::Node& pluginsNode, juce::ValueTree& trackState);

private:
    static YAML::Node emitAudioClip (const juce::ValueTree& clipState, const juce::File& sessionDir);
    static YAML::Node emitMidiClip (const juce::ValueTree& clipState);

    static juce::ValueTree parseAudioClip (const YAML::Node& node, const juce::File& sessionDir);
    static juce::ValueTree parseMidiClip (const YAML::Node& node);

    static juce::String colourToHex (juce::Colour c);
    static juce::Colour hexToColour (const juce::String& hex);

    static juce::String makeRelativePath (const juce::File& file, const juce::File& sessionDir);
    static juce::File resolveRelativePath (const juce::String& relativePath, const juce::File& sessionDir);

    static YAML::Node emitStepPattern (const juce::ValueTree& patternState);
    static YAML::Node emitStepRow (const juce::ValueTree& rowState);
    static juce::ValueTree parseStepPattern (const YAML::Node& node);
    static juce::ValueTree parseStepRow (const YAML::Node& node);
};

} // namespace dc
