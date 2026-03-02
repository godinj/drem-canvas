#include "Track.h"
#include "dc/foundation/assert.h"

namespace dc
{

Track::Track (const juce::ValueTree& s)
    : state (s)
{
    dc_assert (state.hasType (IDs::TRACK));
}

std::string Track::getName() const
{
    return state.getProperty (IDs::name, "").toString().toStdString();
}

void Track::setName (const std::string& n, juce::UndoManager* um)
{
    state.setProperty (IDs::name, n.c_str(), um);
}

float Track::getVolume() const
{
    return state.getProperty (IDs::volume, 1.0f);
}

void Track::setVolume (float vol, juce::UndoManager* um)
{
    state.setProperty (IDs::volume, vol, um);
}

float Track::getPan() const
{
    return state.getProperty (IDs::pan, 0.0f);
}

void Track::setPan (float p, juce::UndoManager* um)
{
    state.setProperty (IDs::pan, p, um);
}

bool Track::isMuted() const
{
    return state.getProperty (IDs::mute, false);
}

void Track::setMuted (bool m, juce::UndoManager* um)
{
    state.setProperty (IDs::mute, m, um);
}

bool Track::isSolo() const
{
    return state.getProperty (IDs::solo, false);
}

void Track::setSolo (bool s, juce::UndoManager* um)
{
    state.setProperty (IDs::solo, s, um);
}

bool Track::isArmed() const
{
    return state.getProperty (IDs::armed, false);
}

void Track::setArmed (bool a, juce::UndoManager* um)
{
    state.setProperty (IDs::armed, a, um);
}

dc::Colour Track::getColour() const
{
    return dc::Colour (static_cast<uint32_t> (static_cast<int> (state.getProperty (IDs::colour, 0))));
}

juce::ValueTree Track::addAudioClip (const std::filesystem::path& sourceFile, int64_t startPosition, int64_t length)
{
    juce::ValueTree clip (IDs::AUDIO_CLIP);
    clip.setProperty (IDs::sourceFile, sourceFile.string().c_str(), nullptr);
    clip.setProperty (IDs::startPosition, static_cast<juce::int64> (startPosition), nullptr);
    clip.setProperty (IDs::length, static_cast<juce::int64> (length), nullptr);
    clip.setProperty (IDs::trimStart, static_cast<juce::int64> (0), nullptr);
    clip.setProperty (IDs::trimEnd, static_cast<juce::int64> (length), nullptr);
    clip.setProperty (IDs::fadeInLength, static_cast<juce::int64> (0), nullptr);
    clip.setProperty (IDs::fadeOutLength, static_cast<juce::int64> (0), nullptr);

    state.appendChild (clip, nullptr);
    return clip;
}

juce::ValueTree Track::addMidiClip (int64_t startPosition, int64_t length)
{
    juce::ValueTree clip (IDs::MIDI_CLIP);
    clip.setProperty (IDs::startPosition, static_cast<juce::int64> (startPosition), nullptr);
    clip.setProperty (IDs::length, static_cast<juce::int64> (length), nullptr);

    state.appendChild (clip, nullptr);
    return clip;
}

int Track::getNumClips() const
{
    int count = 0;
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.hasType (IDs::AUDIO_CLIP) || child.hasType (IDs::MIDI_CLIP))
            ++count;
    }
    return count;
}

juce::ValueTree Track::getClip (int index) const
{
    int count = 0;
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.hasType (IDs::AUDIO_CLIP) || child.hasType (IDs::MIDI_CLIP))
        {
            if (count == index)
                return child;
            ++count;
        }
    }
    return {};
}

void Track::removeClip (int index, juce::UndoManager* um)
{
    int count = 0;
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.hasType (IDs::AUDIO_CLIP) || child.hasType (IDs::MIDI_CLIP))
        {
            if (count == index)
            {
                state.removeChild (i, um);
                return;
            }
            ++count;
        }
    }
}

// ── Plugin chain management ─────────────────────────────────────────────────

juce::ValueTree Track::getPluginChain()
{
    auto chain = state.getChildWithName (IDs::PLUGIN_CHAIN);
    if (! chain.isValid())
    {
        chain = juce::ValueTree (IDs::PLUGIN_CHAIN);
        state.appendChild (chain, nullptr);
    }
    return chain;
}

juce::ValueTree Track::addPlugin (const std::string& name, const std::string& format,
                                   const std::string& manufacturer, int uniqueId,
                                   const std::string& fileOrIdentifier,
                                   juce::UndoManager* um)
{
    juce::ValueTree plugin (IDs::PLUGIN);
    plugin.setProperty (IDs::pluginName, name.c_str(), nullptr);
    plugin.setProperty (IDs::pluginFormat, format.c_str(), nullptr);
    plugin.setProperty (IDs::pluginManufacturer, manufacturer.c_str(), nullptr);
    plugin.setProperty (IDs::pluginUniqueId, uniqueId, nullptr);
    plugin.setProperty (IDs::pluginFileOrIdentifier, fileOrIdentifier.c_str(), nullptr);
    plugin.setProperty (IDs::pluginState, "", nullptr);
    plugin.setProperty (IDs::pluginEnabled, true, nullptr);

    getPluginChain().appendChild (plugin, um);
    return plugin;
}

void Track::removePlugin (int index, juce::UndoManager* um)
{
    auto chain = getPluginChain();
    if (index >= 0 && index < chain.getNumChildren())
        chain.removeChild (index, um);
}

void Track::movePlugin (int fromIndex, int toIndex, juce::UndoManager* um)
{
    auto chain = getPluginChain();
    if (fromIndex >= 0 && fromIndex < chain.getNumChildren()
        && toIndex >= 0 && toIndex < chain.getNumChildren()
        && fromIndex != toIndex)
    {
        chain.moveChild (fromIndex, toIndex, um);
    }
}

int Track::getNumPlugins() const
{
    auto chain = state.getChildWithName (IDs::PLUGIN_CHAIN);
    return chain.isValid() ? chain.getNumChildren() : 0;
}

juce::ValueTree Track::getPlugin (int index) const
{
    auto chain = state.getChildWithName (IDs::PLUGIN_CHAIN);
    return chain.isValid() ? chain.getChild (index) : juce::ValueTree();
}

void Track::setPluginEnabled (int index, bool enabled, juce::UndoManager* um)
{
    auto plugin = getPlugin (index);
    if (plugin.isValid())
        plugin.setProperty (IDs::pluginEnabled, enabled, um);
}

bool Track::isPluginEnabled (int index) const
{
    auto plugin = getPlugin (index);
    return plugin.isValid() ? static_cast<bool> (plugin.getProperty (IDs::pluginEnabled, true)) : false;
}

void Track::setPluginState (int index, const std::string& base64State, juce::UndoManager* um)
{
    auto plugin = getPlugin (index);
    if (plugin.isValid())
        plugin.setProperty (IDs::pluginState, base64State.c_str(), um);
}

} // namespace dc
