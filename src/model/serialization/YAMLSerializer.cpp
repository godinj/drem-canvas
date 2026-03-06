#include "YAMLSerializer.h"
#include "model/Project.h"

namespace dc
{

namespace
{
    const dc::PropertyId masterVolumeId ("masterVolume");
    const dc::PropertyId midiDataId ("midiData");
}

// --- Helpers ---

std::string YAMLSerializer::colourToHex (dc::Colour c)
{
    return c.toHexString();
}

dc::Colour YAMLSerializer::hexToColour (const std::string& hex)
{
    return dc::Colour::fromHexString (hex);
}

std::string YAMLSerializer::makeRelativePath (const std::filesystem::path& file, const std::filesystem::path& sessionDir)
{
    return std::filesystem::proximate (file, sessionDir).string();
}

std::filesystem::path YAMLSerializer::resolveRelativePath (const std::string& relativePath, const std::filesystem::path& sessionDir)
{
    return sessionDir / relativePath;
}

// --- Emit ---

YAML::Node YAMLSerializer::emitSessionMeta (const PropertyTree& projectState, int trackCount)
{
    YAML::Node root;
    root["drem_canvas_version"] = "0.1.0";

    YAML::Node proj;
    proj["tempo"] = projectState.getProperty (IDs::tempo, Variant (120.0)).toDouble();

    YAML::Node timeSig;
    timeSig["numerator"] = static_cast<int> (projectState.getProperty (IDs::timeSigNumerator, Variant (4)).toInt());
    timeSig["denominator"] = static_cast<int> (projectState.getProperty (IDs::timeSigDenominator, Variant (4)).toInt());
    proj["time_signature"] = timeSig;

    proj["sample_rate"] = projectState.getProperty (IDs::sampleRate, Variant (44100.0)).toDouble();
    proj["master_volume"] = projectState.getProperty (masterVolumeId, Variant (1.0)).toDouble();
    proj["cycle_enabled"] = projectState.getProperty (IDs::cycleEnabled, Variant (false)).toBool();
    proj["cycle_start"]   = projectState.getProperty (IDs::cycleStart, Variant (int64_t (0))).toInt();
    proj["cycle_end"]     = projectState.getProperty (IDs::cycleEnd, Variant (int64_t (0))).toInt();

    root["project"] = proj;
    root["track_count"] = trackCount;

    return root;
}

YAML::Node YAMLSerializer::emitTrack (const PropertyTree& trackState, const std::filesystem::path& sessionDir)
{
    YAML::Node root;
    YAML::Node track;

    track["name"] = trackState.getProperty (IDs::name, Variant ("")).toString();

    auto colour = dc::Colour (static_cast<uint32_t> (trackState.getProperty (IDs::colour, Variant (0)).toInt()));
    track["colour"] = colourToHex (colour);

    YAML::Node mixer;
    mixer["volume"] = trackState.getProperty (IDs::volume, Variant (1.0)).toDouble();
    mixer["pan"] = trackState.getProperty (IDs::pan, Variant (0.0)).toDouble();
    mixer["mute"] = trackState.getProperty (IDs::mute, Variant (false)).toBool();
    mixer["solo"] = trackState.getProperty (IDs::solo, Variant (false)).toBool();
    mixer["armed"] = trackState.getProperty (IDs::armed, Variant (false)).toBool();
    track["mixer"] = mixer;

    YAML::Node clips;
    for (int i = 0; i < trackState.getNumChildren(); ++i)
    {
        auto child = trackState.getChild (i);
        if (child.getType() == IDs::AUDIO_CLIP)
            clips.push_back (emitAudioClip (child, sessionDir));
        else if (child.getType() == IDs::MIDI_CLIP)
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

YAML::Node YAMLSerializer::emitAudioClip (const PropertyTree& clipState, const std::filesystem::path& sessionDir)
{
    YAML::Node clip;
    clip["type"] = "audio";

    std::filesystem::path sourceFile (clipState.getProperty (IDs::sourceFile, Variant ("")).toString());
    clip["source_file"] = makeRelativePath (sourceFile, sessionDir);

    clip["start_position"] = clipState.getProperty (IDs::startPosition, Variant (0)).toInt();
    clip["length"] = clipState.getProperty (IDs::length, Variant (0)).toInt();
    clip["trim_start"] = clipState.getProperty (IDs::trimStart, Variant (0)).toInt();
    clip["trim_end"] = clipState.getProperty (IDs::trimEnd, Variant (0)).toInt();
    clip["fade_in_length"] = clipState.getProperty (IDs::fadeInLength, Variant (0)).toInt();
    clip["fade_out_length"] = clipState.getProperty (IDs::fadeOutLength, Variant (0)).toInt();

    return clip;
}

YAML::Node YAMLSerializer::emitMidiClip (const PropertyTree& clipState)
{
    YAML::Node clip;
    clip["type"] = "midi";

    clip["start_position"] = clipState.getProperty (IDs::startPosition, Variant (0)).toInt();
    clip["length"] = clipState.getProperty (IDs::length, Variant (0)).toInt();

    auto base64Data = clipState.getProperty (midiDataId, Variant ("")).toString();
    clip["midi_data"] = base64Data;

    return clip;
}

// --- Parse ---

PropertyTree YAMLSerializer::parseSessionMeta (const YAML::Node& node)
{
    PropertyTree state (IDs::PROJECT);
    state.addChild (PropertyTree (IDs::TRACKS), -1);

    if (auto proj = node["project"])
    {
        if (proj["tempo"])
            state.setProperty (IDs::tempo, Variant (proj["tempo"].as<double>()));

        if (auto ts = proj["time_signature"])
        {
            if (ts["numerator"])
                state.setProperty (IDs::timeSigNumerator, Variant (ts["numerator"].as<int>()));
            if (ts["denominator"])
                state.setProperty (IDs::timeSigDenominator, Variant (ts["denominator"].as<int>()));
        }

        if (proj["sample_rate"])
            state.setProperty (IDs::sampleRate, Variant (proj["sample_rate"].as<double>()));

        if (proj["master_volume"])
            state.setProperty (masterVolumeId, Variant (proj["master_volume"].as<double>()));

        if (proj["cycle_enabled"])
            state.setProperty (IDs::cycleEnabled, Variant (proj["cycle_enabled"].as<bool>()));
        if (proj["cycle_start"])
            state.setProperty (IDs::cycleStart, Variant (proj["cycle_start"].as<int64_t>()));
        if (proj["cycle_end"])
            state.setProperty (IDs::cycleEnd, Variant (proj["cycle_end"].as<int64_t>()));
    }

    return state;
}

PropertyTree YAMLSerializer::parseTrack (const YAML::Node& node, const std::filesystem::path& sessionDir)
{
    PropertyTree trackState (IDs::TRACK);

    auto track = node["track"];
    if (! track)
        return trackState;

    if (track["name"])
        trackState.setProperty (IDs::name, Variant (track["name"].as<std::string>()));

    if (track["colour"])
        trackState.setProperty (IDs::colour, Variant (static_cast<int> (hexToColour (track["colour"].as<std::string>()).argb)));

    if (auto mixer = track["mixer"])
    {
        if (mixer["volume"])
            trackState.setProperty (IDs::volume, Variant (mixer["volume"].as<double>()));
        if (mixer["pan"])
            trackState.setProperty (IDs::pan, Variant (mixer["pan"].as<double>()));
        if (mixer["mute"])
            trackState.setProperty (IDs::mute, Variant (mixer["mute"].as<bool>()));
        if (mixer["solo"])
            trackState.setProperty (IDs::solo, Variant (mixer["solo"].as<bool>()));
        if (mixer["armed"])
            trackState.setProperty (IDs::armed, Variant (mixer["armed"].as<bool>()));
    }

    if (auto clips = track["clips"])
    {
        for (std::size_t i = 0; i < clips.size(); ++i)
        {
            auto clipNode = clips[i];
            auto type = clipNode["type"].as<std::string>();

            if (type == "audio")
                trackState.addChild (parseAudioClip (clipNode, sessionDir), -1);
            else if (type == "midi")
                trackState.addChild (parseMidiClip (clipNode), -1);
        }
    }

    if (auto plugins = track["plugins"])
        parsePluginChain (plugins, trackState);

    return trackState;
}

// --- Plugin chain ---

YAML::Node YAMLSerializer::emitPluginChain (const PropertyTree& trackState)
{
    YAML::Node plugins;
    auto chain = trackState.getChildWithType (IDs::PLUGIN_CHAIN);

    if (! chain.isValid())
        return plugins;

    for (int i = 0; i < chain.getNumChildren(); ++i)
    {
        auto pluginState = chain.getChild (i);
        if (pluginState.getType() != IDs::PLUGIN)
            continue;

        YAML::Node p;
        p["name"]               = pluginState.getProperty (IDs::pluginName, Variant ("")).toString();
        p["format"]             = pluginState.getProperty (IDs::pluginFormat, Variant ("")).toString();
        p["manufacturer"]       = pluginState.getProperty (IDs::pluginManufacturer, Variant ("")).toString();
        p["unique_id"]          = static_cast<int> (pluginState.getProperty (IDs::pluginUniqueId, Variant (0)).toInt());
        p["file_or_identifier"] = pluginState.getProperty (IDs::pluginFileOrIdentifier, Variant ("")).toString();
        p["state"]              = pluginState.getProperty (IDs::pluginState, Variant ("")).toString();
        p["enabled"]            = pluginState.getProperty (IDs::pluginEnabled, Variant (true)).toBool();

        plugins.push_back (p);
    }

    return plugins;
}

void YAMLSerializer::parsePluginChain (const YAML::Node& pluginsNode, PropertyTree& trackState)
{
    PropertyTree chain (IDs::PLUGIN_CHAIN);

    for (std::size_t i = 0; i < pluginsNode.size(); ++i)
    {
        auto p = pluginsNode[i];
        PropertyTree plugin (IDs::PLUGIN);

        if (p["name"])
            plugin.setProperty (IDs::pluginName, Variant (p["name"].as<std::string>()));
        if (p["format"])
            plugin.setProperty (IDs::pluginFormat, Variant (p["format"].as<std::string>()));
        if (p["manufacturer"])
            plugin.setProperty (IDs::pluginManufacturer, Variant (p["manufacturer"].as<std::string>()));
        if (p["unique_id"])
            plugin.setProperty (IDs::pluginUniqueId, Variant (p["unique_id"].as<int>()));
        if (p["file_or_identifier"])
            plugin.setProperty (IDs::pluginFileOrIdentifier, Variant (p["file_or_identifier"].as<std::string>()));
        if (p["state"])
            plugin.setProperty (IDs::pluginState, Variant (p["state"].as<std::string>()));
        if (p["enabled"])
            plugin.setProperty (IDs::pluginEnabled, Variant (p["enabled"].as<bool>()));

        chain.addChild (plugin, -1);
    }

    trackState.addChild (chain, -1);
}

PropertyTree YAMLSerializer::parseAudioClip (const YAML::Node& node, const std::filesystem::path& sessionDir)
{
    PropertyTree clip (IDs::AUDIO_CLIP);

    if (node["source_file"])
    {
        auto resolved = resolveRelativePath (node["source_file"].as<std::string>(), sessionDir);
        clip.setProperty (IDs::sourceFile, Variant (resolved.string()));
    }

    if (node["start_position"])
        clip.setProperty (IDs::startPosition, Variant (node["start_position"].as<int64_t>()));
    if (node["length"])
        clip.setProperty (IDs::length, Variant (node["length"].as<int64_t>()));
    if (node["trim_start"])
        clip.setProperty (IDs::trimStart, Variant (node["trim_start"].as<int64_t>()));
    if (node["trim_end"])
        clip.setProperty (IDs::trimEnd, Variant (node["trim_end"].as<int64_t>()));
    if (node["fade_in_length"])
        clip.setProperty (IDs::fadeInLength, Variant (node["fade_in_length"].as<int64_t>()));
    if (node["fade_out_length"])
        clip.setProperty (IDs::fadeOutLength, Variant (node["fade_out_length"].as<int64_t>()));

    return clip;
}

PropertyTree YAMLSerializer::parseMidiClip (const YAML::Node& node)
{
    PropertyTree clip (IDs::MIDI_CLIP);

    if (node["start_position"])
        clip.setProperty (IDs::startPosition, Variant (node["start_position"].as<int64_t>()));
    if (node["length"])
        clip.setProperty (IDs::length, Variant (node["length"].as<int64_t>()));
    if (node["midi_data"])
        clip.setProperty (midiDataId, Variant (node["midi_data"].as<std::string>()));

    return clip;
}

// --- Step Sequencer Emit ---

YAML::Node YAMLSerializer::emitStepSequencer (const PropertyTree& sequencerState)
{
    YAML::Node root;
    YAML::Node seq;

    seq["num_steps"] = static_cast<int> (sequencerState.getProperty (IDs::numSteps, Variant (16)).toInt());
    seq["swing"] = sequencerState.getProperty (IDs::swing, Variant (0.0)).toDouble();
    seq["active_pattern_bank"] = static_cast<int> (sequencerState.getProperty (IDs::activePatternBank, Variant (0)).toInt());
    seq["active_pattern_slot"] = static_cast<int> (sequencerState.getProperty (IDs::activePatternSlot, Variant (0)).toInt());

    YAML::Node patterns;
    for (int i = 0; i < sequencerState.getNumChildren(); ++i)
    {
        auto child = sequencerState.getChild (i);
        if (child.getType() == IDs::STEP_PATTERN)
            patterns.push_back (emitStepPattern (child));
    }
    seq["patterns"] = patterns;

    root["step_sequencer"] = seq;
    return root;
}

YAML::Node YAMLSerializer::emitStepPattern (const PropertyTree& patternState)
{
    YAML::Node pattern;
    pattern["bank"] = static_cast<int> (patternState.getProperty (IDs::bank, Variant (0)).toInt());
    pattern["slot"] = static_cast<int> (patternState.getProperty (IDs::slot, Variant (0)).toInt());
    pattern["name"] = patternState.getProperty (IDs::name, Variant ("?")).toString();
    pattern["num_steps"] = static_cast<int> (patternState.getProperty (IDs::numSteps, Variant (16)).toInt());
    pattern["step_division"] = static_cast<int> (patternState.getProperty (IDs::stepDivision, Variant (4)).toInt());

    YAML::Node rows;
    for (int i = 0; i < patternState.getNumChildren(); ++i)
    {
        auto child = patternState.getChild (i);
        if (child.getType() == IDs::STEP_ROW)
            rows.push_back (emitStepRow (child));
    }
    pattern["rows"] = rows;

    return pattern;
}

YAML::Node YAMLSerializer::emitStepRow (const PropertyTree& rowState)
{
    YAML::Node row;
    row["note_number"] = static_cast<int> (rowState.getProperty (IDs::noteNumber, Variant (36)).toInt());
    row["name"] = rowState.getProperty (IDs::name, Variant ("---")).toString();
    row["mute"] = rowState.getProperty (IDs::mute, Variant (false)).toBool();
    row["solo"] = rowState.getProperty (IDs::solo, Variant (false)).toBool();

    YAML::Node steps;
    for (int i = 0; i < rowState.getNumChildren(); ++i)
    {
        auto child = rowState.getChild (i);
        if (child.getType() == IDs::STEP)
        {
            YAML::Node step;
            step["index"] = static_cast<int> (child.getProperty (IDs::index, Variant (0)).toInt());
            step["active"] = child.getProperty (IDs::active, Variant (false)).toBool();
            step["velocity"] = static_cast<int> (child.getProperty (IDs::velocity, Variant (100)).toInt());
            step["probability"] = child.getProperty (IDs::probability, Variant (1.0)).toDouble();
            step["note_length"] = child.getProperty (IDs::noteLength, Variant (1.0)).toDouble();
            steps.push_back (step);
        }
    }
    row["steps"] = steps;

    return row;
}

// --- Step Sequencer Parse ---

PropertyTree YAMLSerializer::parseStepSequencer (const YAML::Node& node)
{
    PropertyTree state (IDs::STEP_SEQUENCER);

    auto seq = node["step_sequencer"];
    if (! seq)
        return state;

    if (seq["num_steps"])
        state.setProperty (IDs::numSteps, Variant (seq["num_steps"].as<int>()));
    if (seq["swing"])
        state.setProperty (IDs::swing, Variant (seq["swing"].as<double>()));
    if (seq["active_pattern_bank"])
        state.setProperty (IDs::activePatternBank, Variant (seq["active_pattern_bank"].as<int>()));
    if (seq["active_pattern_slot"])
        state.setProperty (IDs::activePatternSlot, Variant (seq["active_pattern_slot"].as<int>()));

    if (auto patterns = seq["patterns"])
    {
        for (std::size_t i = 0; i < patterns.size(); ++i)
            state.addChild (parseStepPattern (patterns[i]), -1);
    }

    return state;
}

PropertyTree YAMLSerializer::parseStepPattern (const YAML::Node& node)
{
    PropertyTree pattern (IDs::STEP_PATTERN);

    if (node["bank"])
        pattern.setProperty (IDs::bank, Variant (node["bank"].as<int>()));
    if (node["slot"])
        pattern.setProperty (IDs::slot, Variant (node["slot"].as<int>()));
    if (node["name"])
        pattern.setProperty (IDs::name, Variant (node["name"].as<std::string>()));
    if (node["num_steps"])
        pattern.setProperty (IDs::numSteps, Variant (node["num_steps"].as<int>()));
    if (node["step_division"])
        pattern.setProperty (IDs::stepDivision, Variant (node["step_division"].as<int>()));

    if (auto rows = node["rows"])
    {
        for (std::size_t i = 0; i < rows.size(); ++i)
            pattern.addChild (parseStepRow (rows[i]), -1);
    }

    return pattern;
}

PropertyTree YAMLSerializer::parseStepRow (const YAML::Node& node)
{
    PropertyTree row (IDs::STEP_ROW);

    if (node["note_number"])
        row.setProperty (IDs::noteNumber, Variant (node["note_number"].as<int>()));
    if (node["name"])
        row.setProperty (IDs::name, Variant (node["name"].as<std::string>()));
    if (node["mute"])
        row.setProperty (IDs::mute, Variant (node["mute"].as<bool>()));
    if (node["solo"])
        row.setProperty (IDs::solo, Variant (node["solo"].as<bool>()));

    if (auto steps = node["steps"])
    {
        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            auto stepNode = steps[i];
            PropertyTree step (IDs::STEP);

            if (stepNode["index"])
                step.setProperty (IDs::index, Variant (stepNode["index"].as<int>()));
            if (stepNode["active"])
                step.setProperty (IDs::active, Variant (stepNode["active"].as<bool>()));
            if (stepNode["velocity"])
                step.setProperty (IDs::velocity, Variant (stepNode["velocity"].as<int>()));
            if (stepNode["probability"])
                step.setProperty (IDs::probability, Variant (stepNode["probability"].as<double>()));
            if (stepNode["note_length"])
                step.setProperty (IDs::noteLength, Variant (stepNode["note_length"].as<double>()));

            row.addChild (step, -1);
        }
    }

    return row;
}

} // namespace dc
