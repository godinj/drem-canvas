#include "Track.h"

namespace dc
{

Track::Track (const juce::ValueTree& s)
    : state (s)
{
    jassert (state.hasType (IDs::TRACK));
}

juce::String Track::getName() const
{
    return state.getProperty (IDs::name, juce::String());
}

void Track::setName (const juce::String& n, juce::UndoManager* um)
{
    state.setProperty (IDs::name, n, um);
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

juce::Colour Track::getColour() const
{
    return juce::Colour (static_cast<juce::uint32> (static_cast<int> (state.getProperty (IDs::colour, 0))));
}

juce::ValueTree Track::addAudioClip (const juce::File& sourceFile, int64_t startPosition, int64_t length)
{
    juce::ValueTree clip (IDs::AUDIO_CLIP);
    clip.setProperty (IDs::sourceFile, sourceFile.getFullPathName(), nullptr);
    clip.setProperty (IDs::startPosition, static_cast<juce::int64> (startPosition), nullptr);
    clip.setProperty (IDs::length, static_cast<juce::int64> (length), nullptr);
    clip.setProperty (IDs::trimStart, static_cast<juce::int64> (0), nullptr);
    clip.setProperty (IDs::trimEnd, static_cast<juce::int64> (length), nullptr);
    clip.setProperty (IDs::fadeInLength, static_cast<juce::int64> (0), nullptr);
    clip.setProperty (IDs::fadeOutLength, static_cast<juce::int64> (0), nullptr);

    state.appendChild (clip, nullptr);
    return clip;
}

int Track::getNumClips() const
{
    return state.getNumChildren();
}

juce::ValueTree Track::getClip (int index) const
{
    return state.getChild (index);
}

void Track::removeClip (int index)
{
    state.removeChild (index, nullptr);
}

} // namespace dc
