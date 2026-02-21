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

private:
    static YAML::Node emitAudioClip (const juce::ValueTree& clipState, const juce::File& sessionDir);
    static YAML::Node emitMidiClip (const juce::ValueTree& clipState);

    static juce::ValueTree parseAudioClip (const YAML::Node& node, const juce::File& sessionDir);
    static juce::ValueTree parseMidiClip (const YAML::Node& node);

    static juce::String colourToHex (juce::Colour c);
    static juce::Colour hexToColour (const juce::String& hex);

    static juce::String makeRelativePath (const juce::File& file, const juce::File& sessionDir);
    static juce::File resolveRelativePath (const juce::String& relativePath, const juce::File& sessionDir);
};

} // namespace dc
