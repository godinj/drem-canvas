#include "Project.h"
#include "StepSequencer.h"
#include "serialization/SessionWriter.h"
#include "serialization/SessionReader.h"
#include "dc/foundation/types.h"
#include "dc/foundation/file_utils.h"

namespace dc
{

Project::Project()
{
    createDefaultState();
}

void Project::createDefaultState()
{
    state = PropertyTree (IDs::PROJECT);
    state.addChild (PropertyTree (IDs::TRACKS), -1, nullptr);
    state.setProperty (IDs::tempo, Variant (120.0), nullptr);
    state.setProperty (IDs::timeSigNumerator, Variant (4), nullptr);
    state.setProperty (IDs::timeSigDenominator, Variant (4), nullptr);
    state.setProperty (IDs::sampleRate, Variant (44100.0), nullptr);
    // Master bus with volume and empty plugin chain
    auto masterBus = PropertyTree (IDs::MASTER_BUS);
    masterBus.setProperty (IDs::volume, Variant (1.0), nullptr);
    masterBus.addChild (PropertyTree (IDs::PLUGIN_CHAIN), -1, nullptr);
    state.addChild (masterBus, -1, nullptr);

    state.addChild (StepSequencer::createDefaultState(), -1, nullptr);
}

bool Project::saveToFile (const std::filesystem::path& /*file*/) const
{
    // TODO: XML serialization removed in sans-JUCE migration
    return false;
}

bool Project::loadFromFile (const std::filesystem::path& /*file*/)
{
    // TODO: XML serialization removed in sans-JUCE migration
    return false;
}

PropertyTree Project::addTrack (const std::string& trackName)
{
    PropertyTree track (IDs::TRACK);
    track.setProperty (IDs::name, Variant (trackName), nullptr);
    track.setProperty (IDs::volume, Variant (1.0), nullptr);
    track.setProperty (IDs::pan, Variant (0.0), nullptr);
    track.setProperty (IDs::mute, Variant (false), nullptr);
    track.setProperty (IDs::solo, Variant (false), nullptr);
    track.setProperty (IDs::armed, Variant (false), nullptr);
    track.setProperty (IDs::colour, Variant (dc::randomInt (0, 0x7FFFFFFF)), nullptr);

    state.getChildWithType (IDs::TRACKS).addChild (track, -1, &undoSystem.getUndoManager());
    return track;
}

void Project::removeTrack (int index)
{
    auto tracks = state.getChildWithType (IDs::TRACKS);
    tracks.removeChild (index, &undoSystem.getUndoManager());
}

int Project::getNumTracks() const
{
    return state.getChildWithType (IDs::TRACKS).getNumChildren();
}

PropertyTree Project::getTrack (int index) const
{
    return state.getChildWithType (IDs::TRACKS).getChild (index);
}

PropertyTree Project::getMasterBusState()
{
    auto masterBus = state.getChildWithType (IDs::MASTER_BUS);
    if (! masterBus.isValid())
    {
        // Create on demand for backward compatibility with old sessions
        masterBus = PropertyTree (IDs::MASTER_BUS);
        masterBus.setProperty (IDs::volume, Variant (1.0), nullptr);
        masterBus.addChild (PropertyTree (IDs::PLUGIN_CHAIN), -1, nullptr);
        state.addChild (masterBus, -1, nullptr);
    }
    return masterBus;
}

double Project::getTempo() const
{
    return state.getProperty (IDs::tempo).getDoubleOr (120.0);
}

void Project::setTempo (double bpm)
{
    state.setProperty (IDs::tempo, Variant (bpm), &undoSystem.getUndoManager());
}

double Project::getSampleRate() const
{
    return state.getProperty (IDs::sampleRate).getDoubleOr (44100.0);
}

void Project::setSampleRate (double sr)
{
    state.setProperty (IDs::sampleRate, Variant (sr), &undoSystem.getUndoManager());
}

int Project::getTimeSigNumerator() const
{
    return static_cast<int> (state.getProperty (IDs::timeSigNumerator).getIntOr (4));
}

void Project::setTimeSigNumerator (int num)
{
    state.setProperty (IDs::timeSigNumerator, Variant (num), &undoSystem.getUndoManager());
}

int Project::getTimeSigDenominator() const
{
    return static_cast<int> (state.getProperty (IDs::timeSigDenominator).getIntOr (4));
}

void Project::setTimeSigDenominator (int den)
{
    state.setProperty (IDs::timeSigDenominator, Variant (den), &undoSystem.getUndoManager());
}

bool Project::saveSessionToDirectory (const std::filesystem::path& sessionDir) const
{
    return SessionWriter::writeSession (state, sessionDir);
}

bool Project::loadSessionFromDirectory (const std::filesystem::path& sessionDir)
{
    auto newState = SessionReader::readSession (sessionDir);

    if (! newState.isValid() || ! (newState.getType() == IDs::PROJECT))
        return false;

    state = newState;
    return true;
}

} // namespace dc
