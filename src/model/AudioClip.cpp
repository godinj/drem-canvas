#include "AudioClip.h"
#include "dc/foundation/assert.h"

namespace dc
{

AudioClip::AudioClip (const PropertyTree& s)
    : state (s)
{
    dc_assert (state.getType() == IDs::AUDIO_CLIP);
}

std::filesystem::path AudioClip::getSourceFile() const
{
    return std::filesystem::path (state.getProperty (IDs::sourceFile).getStringOr (""));
}

int64_t AudioClip::getStartPosition() const
{
    return state.getProperty (IDs::startPosition).getIntOr (0);
}

void AudioClip::setStartPosition (int64_t pos, UndoManager* um)
{
    state.setProperty (IDs::startPosition, Variant (pos), um);
}

int64_t AudioClip::getLength() const
{
    return state.getProperty (IDs::length).getIntOr (0);
}

int64_t AudioClip::getTrimStart() const
{
    return state.getProperty (IDs::trimStart).getIntOr (0);
}

int64_t AudioClip::getTrimEnd() const
{
    return state.getProperty (IDs::trimEnd).getIntOr (0);
}

int64_t AudioClip::getFadeInLength() const
{
    return state.getProperty (IDs::fadeInLength).getIntOr (0);
}

int64_t AudioClip::getFadeOutLength() const
{
    return state.getProperty (IDs::fadeOutLength).getIntOr (0);
}

} // namespace dc
