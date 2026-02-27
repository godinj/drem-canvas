#include "Project.h"
#include "StepSequencer.h"
#include "serialization/SessionWriter.h"
#include "serialization/SessionReader.h"

namespace dc
{

Project::Project()
{
    createDefaultState();
}

void Project::createDefaultState()
{
    state = juce::ValueTree (IDs::PROJECT);
    state.appendChild (juce::ValueTree (IDs::TRACKS), nullptr);
    state.setProperty (IDs::tempo, 120.0, nullptr);
    state.setProperty (IDs::timeSigNumerator, 4, nullptr);
    state.setProperty (IDs::timeSigDenominator, 4, nullptr);
    state.setProperty (IDs::sampleRate, 44100.0, nullptr);
    // Master bus with volume and empty plugin chain
    auto masterBus = juce::ValueTree (IDs::MASTER_BUS);
    masterBus.setProperty (IDs::volume, 1.0, nullptr);
    masterBus.appendChild (juce::ValueTree (IDs::PLUGIN_CHAIN), nullptr);
    state.appendChild (masterBus, nullptr);

    state.appendChild (StepSequencer::createDefaultState(), nullptr);
}

bool Project::saveToFile (const juce::File& file) const
{
    return file.replaceWithText (state.toXmlString());
}

bool Project::loadFromFile (const juce::File& file)
{
    auto xml = juce::parseXML (file);

    if (xml == nullptr)
        return false;

    auto newState = juce::ValueTree::fromXml (*xml);

    if (! newState.hasType (IDs::PROJECT))
        return false;

    state = newState;
    return true;
}

juce::ValueTree Project::addTrack (const juce::String& trackName)
{
    juce::ValueTree track (IDs::TRACK);
    track.setProperty (IDs::name, trackName, nullptr);
    track.setProperty (IDs::volume, 1.0f, nullptr);
    track.setProperty (IDs::pan, 0.0f, nullptr);
    track.setProperty (IDs::mute, false, nullptr);
    track.setProperty (IDs::solo, false, nullptr);
    track.setProperty (IDs::armed, false, nullptr);
    track.setProperty (IDs::colour, static_cast<int> (juce::Random::getSystemRandom().nextInt()), nullptr);

    state.getChildWithName (IDs::TRACKS).appendChild (track, &undoManager);
    return track;
}

void Project::removeTrack (int index)
{
    auto tracks = state.getChildWithName (IDs::TRACKS);
    tracks.removeChild (index, &undoManager);
}

int Project::getNumTracks() const
{
    return state.getChildWithName (IDs::TRACKS).getNumChildren();
}

juce::ValueTree Project::getTrack (int index) const
{
    return state.getChildWithName (IDs::TRACKS).getChild (index);
}

juce::ValueTree Project::getMasterBusState()
{
    auto masterBus = state.getChildWithName (IDs::MASTER_BUS);
    if (! masterBus.isValid())
    {
        // Create on demand for backward compatibility with old sessions
        masterBus = juce::ValueTree (IDs::MASTER_BUS);
        masterBus.setProperty (IDs::volume, 1.0, nullptr);
        masterBus.appendChild (juce::ValueTree (IDs::PLUGIN_CHAIN), nullptr);
        state.appendChild (masterBus, nullptr);
    }
    return masterBus;
}

double Project::getTempo() const
{
    return state.getProperty (IDs::tempo, 120.0);
}

void Project::setTempo (double bpm)
{
    state.setProperty (IDs::tempo, bpm, &undoManager);
}

double Project::getSampleRate() const
{
    return state.getProperty (IDs::sampleRate, 44100.0);
}

void Project::setSampleRate (double sr)
{
    state.setProperty (IDs::sampleRate, sr, &undoManager);
}

int Project::getTimeSigNumerator() const
{
    return state.getProperty (IDs::timeSigNumerator, 4);
}

void Project::setTimeSigNumerator (int num)
{
    state.setProperty (IDs::timeSigNumerator, num, &undoManager);
}

int Project::getTimeSigDenominator() const
{
    return state.getProperty (IDs::timeSigDenominator, 4);
}

void Project::setTimeSigDenominator (int den)
{
    state.setProperty (IDs::timeSigDenominator, den, &undoManager);
}

bool Project::saveSessionToDirectory (const juce::File& sessionDir) const
{
    return SessionWriter::writeSession (state, sessionDir);
}

bool Project::loadSessionFromDirectory (const juce::File& sessionDir)
{
    auto newState = SessionReader::readSession (sessionDir);

    if (! newState.isValid() || ! newState.hasType (IDs::PROJECT))
        return false;

    state = newState;
    return true;
}

} // namespace dc
