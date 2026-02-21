#include "AudioClip.h"

namespace dc
{

AudioClip::AudioClip (const juce::ValueTree& s)
    : state (s)
{
    jassert (state.hasType (IDs::AUDIO_CLIP));
}

juce::File AudioClip::getSourceFile() const
{
    return juce::File (state.getProperty (IDs::sourceFile, juce::String()));
}

int64_t AudioClip::getStartPosition() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::startPosition, 0)));
}

void AudioClip::setStartPosition (int64_t pos, juce::UndoManager* um)
{
    state.setProperty (IDs::startPosition, static_cast<juce::int64> (pos), um);
}

int64_t AudioClip::getLength() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::length, 0)));
}

int64_t AudioClip::getTrimStart() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::trimStart, 0)));
}

int64_t AudioClip::getTrimEnd() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::trimEnd, 0)));
}

int64_t AudioClip::getFadeInLength() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::fadeInLength, 0)));
}

int64_t AudioClip::getFadeOutLength() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::fadeOutLength, 0)));
}

} // namespace dc
