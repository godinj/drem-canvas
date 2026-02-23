#include "YAMLSerializer.h"
#include "model/Project.h"

namespace dc
{

namespace
{
    const juce::Identifier masterVolumeId ("masterVolume");
    const juce::Identifier midiDataId ("midiData");
}

// --- Helpers ---

juce::String YAMLSerializer::colourToHex (juce::Colour c)
{
    return c.toDisplayString (true).toLowerCase();
}

juce::Colour YAMLSerializer::hexToColour (const juce::String& hex)
{
    return juce::Colour (static_cast<juce::uint32> (hex.getHexValue64()));
}

juce::String YAMLSerializer::makeRelativePath (const juce::File& file, const juce::File& sessionDir)
{
    return file.getRelativePathFrom (sessionDir);
}

juce::File YAMLSerializer::resolveRelativePath (const juce::String& relativePath, const juce::File& sessionDir)
{
    return sessionDir.getChildFile (relativePath);
}

// --- Emit ---

YAML::Node YAMLSerializer::emitSessionMeta (const juce::ValueTree& projectState, int trackCount)
{
    YAML::Node root;
    root["drem_canvas_version"] = "0.1.0";

    YAML::Node proj;
    proj["tempo"] = static_cast<double> (projectState.getProperty (IDs::tempo, 120.0));

    YAML::Node timeSig;
    timeSig["numerator"] = static_cast<int> (projectState.getProperty (IDs::timeSigNumerator, 4));
    timeSig["denominator"] = static_cast<int> (projectState.getProperty (IDs::timeSigDenominator, 4));
    proj["time_signature"] = timeSig;

    proj["sample_rate"] = static_cast<double> (projectState.getProperty (IDs::sampleRate, 44100.0));
    proj["master_volume"] = static_cast<double> (static_cast<float> (projectState.getProperty (masterVolumeId, 1.0f)));

    root["project"] = proj;
    root["track_count"] = trackCount;

    return root;
}

YAML::Node YAMLSerializer::emitTrack (const juce::ValueTree& trackState, const juce::File& sessionDir)
{
    YAML::Node root;
    YAML::Node track;

    track["name"] = trackState.getProperty (IDs::name, juce::String()).toString().toStdString();

    auto colour = juce::Colour (static_cast<juce::uint32> (static_cast<int> (trackState.getProperty (IDs::colour, 0))));
    track["colour"] = colourToHex (colour).toStdString();

    YAML::Node mixer;
    mixer["volume"] = static_cast<double> (static_cast<float> (trackState.getProperty (IDs::volume, 1.0f)));
    mixer["pan"] = static_cast<double> (static_cast<float> (trackState.getProperty (IDs::pan, 0.0f)));
    mixer["mute"] = static_cast<bool> (trackState.getProperty (IDs::mute, false));
    mixer["solo"] = static_cast<bool> (trackState.getProperty (IDs::solo, false));
    mixer["armed"] = static_cast<bool> (trackState.getProperty (IDs::armed, false));
    track["mixer"] = mixer;

    YAML::Node clips;
    for (int i = 0; i < trackState.getNumChildren(); ++i)
    {
        auto child = trackState.getChild (i);
        if (child.hasType (IDs::AUDIO_CLIP))
            clips.push_back (emitAudioClip (child, sessionDir));
        else if (child.hasType (IDs::MIDI_CLIP))
            clips.push_back (emitMidiClip (child));
    }
    track["clips"] = clips;

    // Plugin chain
    auto pluginsNode = emitPluginChain (trackState);
    if (pluginsNode.size() > 0)
        track["plugins"] = pluginsNode;

    root["track"] = track;
    return root;
}

YAML::Node YAMLSerializer::emitAudioClip (const juce::ValueTree& clipState, const juce::File& sessionDir)
{
    YAML::Node clip;
    clip["type"] = "audio";

    juce::File sourceFile (clipState.getProperty (IDs::sourceFile, juce::String()).toString());
    clip["source_file"] = makeRelativePath (sourceFile, sessionDir).toStdString();

    clip["start_position"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
    clip["length"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));
    clip["trim_start"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::trimStart, 0)));
    clip["trim_end"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::trimEnd, 0)));
    clip["fade_in_length"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::fadeInLength, 0)));
    clip["fade_out_length"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::fadeOutLength, 0)));

    return clip;
}

YAML::Node YAMLSerializer::emitMidiClip (const juce::ValueTree& clipState)
{
    YAML::Node clip;
    clip["type"] = "midi";

    clip["start_position"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
    clip["length"] = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));

    juce::String base64Data = clipState.getProperty (midiDataId, juce::String());
    clip["midi_data"] = base64Data.toStdString();

    return clip;
}

// --- Parse ---

juce::ValueTree YAMLSerializer::parseSessionMeta (const YAML::Node& node)
{
    juce::ValueTree state (IDs::PROJECT);
    state.appendChild (juce::ValueTree (IDs::TRACKS), nullptr);

    if (auto proj = node["project"])
    {
        if (proj["tempo"])
            state.setProperty (IDs::tempo, proj["tempo"].as<double>(), nullptr);

        if (auto ts = proj["time_signature"])
        {
            if (ts["numerator"])
                state.setProperty (IDs::timeSigNumerator, ts["numerator"].as<int>(), nullptr);
            if (ts["denominator"])
                state.setProperty (IDs::timeSigDenominator, ts["denominator"].as<int>(), nullptr);
        }

        if (proj["sample_rate"])
            state.setProperty (IDs::sampleRate, proj["sample_rate"].as<double>(), nullptr);

        if (proj["master_volume"])
            state.setProperty (masterVolumeId, static_cast<float> (proj["master_volume"].as<double>()), nullptr);
    }

    return state;
}

juce::ValueTree YAMLSerializer::parseTrack (const YAML::Node& node, const juce::File& sessionDir)
{
    juce::ValueTree trackState (IDs::TRACK);

    auto track = node["track"];
    if (! track)
        return trackState;

    if (track["name"])
        trackState.setProperty (IDs::name, juce::String (track["name"].as<std::string>()), nullptr);

    if (track["colour"])
        trackState.setProperty (IDs::colour, static_cast<int> (hexToColour (juce::String (track["colour"].as<std::string>())).getARGB()), nullptr);

    if (auto mixer = track["mixer"])
    {
        if (mixer["volume"])
            trackState.setProperty (IDs::volume, static_cast<float> (mixer["volume"].as<double>()), nullptr);
        if (mixer["pan"])
            trackState.setProperty (IDs::pan, static_cast<float> (mixer["pan"].as<double>()), nullptr);
        if (mixer["mute"])
            trackState.setProperty (IDs::mute, mixer["mute"].as<bool>(), nullptr);
        if (mixer["solo"])
            trackState.setProperty (IDs::solo, mixer["solo"].as<bool>(), nullptr);
        if (mixer["armed"])
            trackState.setProperty (IDs::armed, mixer["armed"].as<bool>(), nullptr);
    }

    if (auto clips = track["clips"])
    {
        for (std::size_t i = 0; i < clips.size(); ++i)
        {
            auto clipNode = clips[i];
            auto type = clipNode["type"].as<std::string>();

            if (type == "audio")
                trackState.appendChild (parseAudioClip (clipNode, sessionDir), nullptr);
            else if (type == "midi")
                trackState.appendChild (parseMidiClip (clipNode), nullptr);
        }
    }

    if (auto plugins = track["plugins"])
        parsePluginChain (plugins, trackState);

    return trackState;
}

// --- Plugin chain ---

YAML::Node YAMLSerializer::emitPluginChain (const juce::ValueTree& trackState)
{
    YAML::Node plugins;
    auto chain = trackState.getChildWithName (IDs::PLUGIN_CHAIN);

    if (! chain.isValid())
        return plugins;

    for (int i = 0; i < chain.getNumChildren(); ++i)
    {
        auto pluginState = chain.getChild (i);
        if (! pluginState.hasType (IDs::PLUGIN))
            continue;

        YAML::Node p;
        p["name"]               = pluginState.getProperty (IDs::pluginName, juce::String()).toString().toStdString();
        p["format"]             = pluginState.getProperty (IDs::pluginFormat, juce::String()).toString().toStdString();
        p["manufacturer"]       = pluginState.getProperty (IDs::pluginManufacturer, juce::String()).toString().toStdString();
        p["unique_id"]          = static_cast<int> (pluginState.getProperty (IDs::pluginUniqueId, 0));
        p["file_or_identifier"] = pluginState.getProperty (IDs::pluginFileOrIdentifier, juce::String()).toString().toStdString();
        p["state"]              = pluginState.getProperty (IDs::pluginState, juce::String()).toString().toStdString();
        p["enabled"]            = static_cast<bool> (pluginState.getProperty (IDs::pluginEnabled, true));

        plugins.push_back (p);
    }

    return plugins;
}

void YAMLSerializer::parsePluginChain (const YAML::Node& pluginsNode, juce::ValueTree& trackState)
{
    juce::ValueTree chain (IDs::PLUGIN_CHAIN);

    for (std::size_t i = 0; i < pluginsNode.size(); ++i)
    {
        auto p = pluginsNode[i];
        juce::ValueTree plugin (IDs::PLUGIN);

        if (p["name"])
            plugin.setProperty (IDs::pluginName, juce::String (p["name"].as<std::string>()), nullptr);
        if (p["format"])
            plugin.setProperty (IDs::pluginFormat, juce::String (p["format"].as<std::string>()), nullptr);
        if (p["manufacturer"])
            plugin.setProperty (IDs::pluginManufacturer, juce::String (p["manufacturer"].as<std::string>()), nullptr);
        if (p["unique_id"])
            plugin.setProperty (IDs::pluginUniqueId, p["unique_id"].as<int>(), nullptr);
        if (p["file_or_identifier"])
            plugin.setProperty (IDs::pluginFileOrIdentifier, juce::String (p["file_or_identifier"].as<std::string>()), nullptr);
        if (p["state"])
            plugin.setProperty (IDs::pluginState, juce::String (p["state"].as<std::string>()), nullptr);
        if (p["enabled"])
            plugin.setProperty (IDs::pluginEnabled, p["enabled"].as<bool>(), nullptr);

        chain.appendChild (plugin, nullptr);
    }

    trackState.appendChild (chain, nullptr);
}

juce::ValueTree YAMLSerializer::parseAudioClip (const YAML::Node& node, const juce::File& sessionDir)
{
    juce::ValueTree clip (IDs::AUDIO_CLIP);

    if (node["source_file"])
    {
        auto resolved = resolveRelativePath (juce::String (node["source_file"].as<std::string>()), sessionDir);
        clip.setProperty (IDs::sourceFile, resolved.getFullPathName(), nullptr);
    }

    if (node["start_position"])
        clip.setProperty (IDs::startPosition, static_cast<juce::int64> (node["start_position"].as<int64_t>()), nullptr);
    if (node["length"])
        clip.setProperty (IDs::length, static_cast<juce::int64> (node["length"].as<int64_t>()), nullptr);
    if (node["trim_start"])
        clip.setProperty (IDs::trimStart, static_cast<juce::int64> (node["trim_start"].as<int64_t>()), nullptr);
    if (node["trim_end"])
        clip.setProperty (IDs::trimEnd, static_cast<juce::int64> (node["trim_end"].as<int64_t>()), nullptr);
    if (node["fade_in_length"])
        clip.setProperty (IDs::fadeInLength, static_cast<juce::int64> (node["fade_in_length"].as<int64_t>()), nullptr);
    if (node["fade_out_length"])
        clip.setProperty (IDs::fadeOutLength, static_cast<juce::int64> (node["fade_out_length"].as<int64_t>()), nullptr);

    return clip;
}

juce::ValueTree YAMLSerializer::parseMidiClip (const YAML::Node& node)
{
    juce::ValueTree clip (IDs::MIDI_CLIP);

    if (node["start_position"])
        clip.setProperty (IDs::startPosition, static_cast<juce::int64> (node["start_position"].as<int64_t>()), nullptr);
    if (node["length"])
        clip.setProperty (IDs::length, static_cast<juce::int64> (node["length"].as<int64_t>()), nullptr);
    if (node["midi_data"])
        clip.setProperty (midiDataId, juce::String (node["midi_data"].as<std::string>()), nullptr);

    return clip;
}

// --- Step Sequencer Emit ---

YAML::Node YAMLSerializer::emitStepSequencer (const juce::ValueTree& sequencerState)
{
    YAML::Node root;
    YAML::Node seq;

    seq["num_steps"] = static_cast<int> (sequencerState.getProperty (IDs::numSteps, 16));
    seq["swing"] = static_cast<double> (sequencerState.getProperty (IDs::swing, 0.0));
    seq["active_pattern_bank"] = static_cast<int> (sequencerState.getProperty (IDs::activePatternBank, 0));
    seq["active_pattern_slot"] = static_cast<int> (sequencerState.getProperty (IDs::activePatternSlot, 0));

    YAML::Node patterns;
    for (int i = 0; i < sequencerState.getNumChildren(); ++i)
    {
        auto child = sequencerState.getChild (i);
        if (child.hasType (IDs::STEP_PATTERN))
            patterns.push_back (emitStepPattern (child));
    }
    seq["patterns"] = patterns;

    root["step_sequencer"] = seq;
    return root;
}

YAML::Node YAMLSerializer::emitStepPattern (const juce::ValueTree& patternState)
{
    YAML::Node pattern;
    pattern["bank"] = static_cast<int> (patternState.getProperty (IDs::bank, 0));
    pattern["slot"] = static_cast<int> (patternState.getProperty (IDs::slot, 0));
    pattern["name"] = patternState.getProperty (IDs::name, juce::String ("?")).toString().toStdString();
    pattern["num_steps"] = static_cast<int> (patternState.getProperty (IDs::numSteps, 16));
    pattern["step_division"] = static_cast<int> (patternState.getProperty (IDs::stepDivision, 4));

    YAML::Node rows;
    for (int i = 0; i < patternState.getNumChildren(); ++i)
    {
        auto child = patternState.getChild (i);
        if (child.hasType (IDs::STEP_ROW))
            rows.push_back (emitStepRow (child));
    }
    pattern["rows"] = rows;

    return pattern;
}

YAML::Node YAMLSerializer::emitStepRow (const juce::ValueTree& rowState)
{
    YAML::Node row;
    row["note_number"] = static_cast<int> (rowState.getProperty (IDs::noteNumber, 36));
    row["name"] = rowState.getProperty (IDs::name, juce::String ("---")).toString().toStdString();
    row["mute"] = static_cast<bool> (rowState.getProperty (IDs::mute, false));
    row["solo"] = static_cast<bool> (rowState.getProperty (IDs::solo, false));

    YAML::Node steps;
    for (int i = 0; i < rowState.getNumChildren(); ++i)
    {
        auto child = rowState.getChild (i);
        if (child.hasType (IDs::STEP))
        {
            YAML::Node step;
            step["index"] = static_cast<int> (child.getProperty (IDs::index, 0));
            step["active"] = static_cast<bool> (child.getProperty (IDs::active, false));
            step["velocity"] = static_cast<int> (child.getProperty (IDs::velocity, 100));
            step["probability"] = static_cast<double> (child.getProperty (IDs::probability, 1.0));
            step["note_length"] = static_cast<double> (child.getProperty (IDs::noteLength, 1.0));
            steps.push_back (step);
        }
    }
    row["steps"] = steps;

    return row;
}

// --- Step Sequencer Parse ---

juce::ValueTree YAMLSerializer::parseStepSequencer (const YAML::Node& node)
{
    juce::ValueTree state (IDs::STEP_SEQUENCER);

    auto seq = node["step_sequencer"];
    if (! seq)
        return state;

    if (seq["num_steps"])
        state.setProperty (IDs::numSteps, seq["num_steps"].as<int>(), nullptr);
    if (seq["swing"])
        state.setProperty (IDs::swing, seq["swing"].as<double>(), nullptr);
    if (seq["active_pattern_bank"])
        state.setProperty (IDs::activePatternBank, seq["active_pattern_bank"].as<int>(), nullptr);
    if (seq["active_pattern_slot"])
        state.setProperty (IDs::activePatternSlot, seq["active_pattern_slot"].as<int>(), nullptr);

    if (auto patterns = seq["patterns"])
    {
        for (std::size_t i = 0; i < patterns.size(); ++i)
            state.appendChild (parseStepPattern (patterns[i]), nullptr);
    }

    return state;
}

juce::ValueTree YAMLSerializer::parseStepPattern (const YAML::Node& node)
{
    juce::ValueTree pattern (IDs::STEP_PATTERN);

    if (node["bank"])
        pattern.setProperty (IDs::bank, node["bank"].as<int>(), nullptr);
    if (node["slot"])
        pattern.setProperty (IDs::slot, node["slot"].as<int>(), nullptr);
    if (node["name"])
        pattern.setProperty (IDs::name, juce::String (node["name"].as<std::string>()), nullptr);
    if (node["num_steps"])
        pattern.setProperty (IDs::numSteps, node["num_steps"].as<int>(), nullptr);
    if (node["step_division"])
        pattern.setProperty (IDs::stepDivision, node["step_division"].as<int>(), nullptr);

    if (auto rows = node["rows"])
    {
        for (std::size_t i = 0; i < rows.size(); ++i)
            pattern.appendChild (parseStepRow (rows[i]), nullptr);
    }

    return pattern;
}

juce::ValueTree YAMLSerializer::parseStepRow (const YAML::Node& node)
{
    juce::ValueTree row (IDs::STEP_ROW);

    if (node["note_number"])
        row.setProperty (IDs::noteNumber, node["note_number"].as<int>(), nullptr);
    if (node["name"])
        row.setProperty (IDs::name, juce::String (node["name"].as<std::string>()), nullptr);
    if (node["mute"])
        row.setProperty (IDs::mute, node["mute"].as<bool>(), nullptr);
    if (node["solo"])
        row.setProperty (IDs::solo, node["solo"].as<bool>(), nullptr);

    if (auto steps = node["steps"])
    {
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            auto stepNode = steps[i];
            juce::ValueTree step (IDs::STEP);

            if (stepNode["index"])
                step.setProperty (IDs::index, stepNode["index"].as<int>(), nullptr);
            if (stepNode["active"])
                step.setProperty (IDs::active, stepNode["active"].as<bool>(), nullptr);
            if (stepNode["velocity"])
                step.setProperty (IDs::velocity, stepNode["velocity"].as<int>(), nullptr);
            if (stepNode["probability"])
                step.setProperty (IDs::probability, stepNode["probability"].as<double>(), nullptr);
            if (stepNode["note_length"])
                step.setProperty (IDs::noteLength, stepNode["note_length"].as<double>(), nullptr);

            row.appendChild (step, nullptr);
        }
    }

    return row;
}

} // namespace dc
