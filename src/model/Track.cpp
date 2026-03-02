#include "Track.h"
#include "dc/foundation/assert.h"

namespace dc
{

Track::Track (const PropertyTree& s)
    : state (s)
{
    dc_assert (state.getType() == IDs::TRACK);
}

std::string Track::getName() const
{
    return state.getProperty (IDs::name).getStringOr ("");
}

void Track::setName (const std::string& n, UndoManager* um)
{
    state.setProperty (IDs::name, Variant (n), um);
}

float Track::getVolume() const
{
    return static_cast<float> (state.getProperty (IDs::volume).getDoubleOr (1.0));
}

void Track::setVolume (float vol, UndoManager* um)
{
    state.setProperty (IDs::volume, Variant (static_cast<double> (vol)), um);
}

float Track::getPan() const
{
    return static_cast<float> (state.getProperty (IDs::pan).getDoubleOr (0.0));
}

void Track::setPan (float p, UndoManager* um)
{
    state.setProperty (IDs::pan, Variant (static_cast<double> (p)), um);
}

bool Track::isMuted() const
{
    return state.getProperty (IDs::mute).getBoolOr (false);
}

void Track::setMuted (bool m, UndoManager* um)
{
    state.setProperty (IDs::mute, Variant (m), um);
}

bool Track::isSolo() const
{
    return state.getProperty (IDs::solo).getBoolOr (false);
}

void Track::setSolo (bool s, UndoManager* um)
{
    state.setProperty (IDs::solo, Variant (s), um);
}

bool Track::isArmed() const
{
    return state.getProperty (IDs::armed).getBoolOr (false);
}

void Track::setArmed (bool a, UndoManager* um)
{
    state.setProperty (IDs::armed, Variant (a), um);
}

dc::Colour Track::getColour() const
{
    return dc::Colour (static_cast<uint32_t> (state.getProperty (IDs::colour).getIntOr (0)));
}

PropertyTree Track::addAudioClip (const std::filesystem::path& sourceFile, int64_t startPosition, int64_t length)
{
    PropertyTree clip (IDs::AUDIO_CLIP);
    clip.setProperty (IDs::sourceFile, Variant (sourceFile.string()), nullptr);
    clip.setProperty (IDs::startPosition, Variant (startPosition), nullptr);
    clip.setProperty (IDs::length, Variant (length), nullptr);
    clip.setProperty (IDs::trimStart, Variant (int64_t (0)), nullptr);
    clip.setProperty (IDs::trimEnd, Variant (length), nullptr);
    clip.setProperty (IDs::fadeInLength, Variant (int64_t (0)), nullptr);
    clip.setProperty (IDs::fadeOutLength, Variant (int64_t (0)), nullptr);

    state.addChild (clip, -1, nullptr);
    return clip;
}

PropertyTree Track::addMidiClip (int64_t startPosition, int64_t length)
{
    PropertyTree clip (IDs::MIDI_CLIP);
    clip.setProperty (IDs::startPosition, Variant (startPosition), nullptr);
    clip.setProperty (IDs::length, Variant (length), nullptr);

    state.addChild (clip, -1, nullptr);
    return clip;
}

int Track::getNumClips() const
{
    int count = 0;
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.getType() == IDs::AUDIO_CLIP || child.getType() == IDs::MIDI_CLIP)
            ++count;
    }
    return count;
}

PropertyTree Track::getClip (int index) const
{
    int count = 0;
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.getType() == IDs::AUDIO_CLIP || child.getType() == IDs::MIDI_CLIP)
        {
            if (count == index)
                return child;
            ++count;
        }
    }
    return {};
}

void Track::removeClip (int index, UndoManager* um)
{
    int count = 0;
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.getType() == IDs::AUDIO_CLIP || child.getType() == IDs::MIDI_CLIP)
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

PropertyTree Track::getPluginChain()
{
    auto chain = state.getChildWithType (IDs::PLUGIN_CHAIN);
    if (! chain.isValid())
    {
        chain = PropertyTree (IDs::PLUGIN_CHAIN);
        state.addChild (chain, -1, nullptr);
    }
    return chain;
}

PropertyTree Track::addPlugin (const std::string& name, const std::string& format,
                               const std::string& manufacturer, int uniqueId,
                               const std::string& fileOrIdentifier,
                               UndoManager* um)
{
    PropertyTree plugin (IDs::PLUGIN);
    plugin.setProperty (IDs::pluginName, Variant (name), nullptr);
    plugin.setProperty (IDs::pluginFormat, Variant (format), nullptr);
    plugin.setProperty (IDs::pluginManufacturer, Variant (manufacturer), nullptr);
    plugin.setProperty (IDs::pluginUniqueId, Variant (uniqueId), nullptr);
    plugin.setProperty (IDs::pluginFileOrIdentifier, Variant (fileOrIdentifier), nullptr);
    plugin.setProperty (IDs::pluginState, Variant (std::string ("")), nullptr);
    plugin.setProperty (IDs::pluginEnabled, Variant (true), nullptr);

    getPluginChain().addChild (plugin, -1, um);
    return plugin;
}

void Track::removePlugin (int index, UndoManager* um)
{
    auto chain = getPluginChain();
    if (index >= 0 && index < chain.getNumChildren())
        chain.removeChild (index, um);
}

void Track::movePlugin (int fromIndex, int toIndex, UndoManager* um)
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
    auto chain = state.getChildWithType (IDs::PLUGIN_CHAIN);
    return chain.isValid() ? chain.getNumChildren() : 0;
}

PropertyTree Track::getPlugin (int index) const
{
    auto chain = state.getChildWithType (IDs::PLUGIN_CHAIN);
    return chain.isValid() ? chain.getChild (index) : PropertyTree();
}

void Track::setPluginEnabled (int index, bool enabled, UndoManager* um)
{
    auto plugin = getPlugin (index);
    if (plugin.isValid())
        plugin.setProperty (IDs::pluginEnabled, Variant (enabled), um);
}

bool Track::isPluginEnabled (int index) const
{
    auto plugin = getPlugin (index);
    return plugin.isValid() ? plugin.getProperty (IDs::pluginEnabled).getBoolOr (true) : false;
}

void Track::setPluginState (int index, const std::string& base64State, UndoManager* um)
{
    auto plugin = getPlugin (index);
    if (plugin.isValid())
        plugin.setProperty (IDs::pluginState, Variant (base64State), um);
}

} // namespace dc
