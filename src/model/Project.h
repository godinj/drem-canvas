#pragma once
#include <JuceHeader.h>
#include "utils/UndoSystem.h"
#include "Clipboard.h"

namespace dc
{

namespace IDs
{
    #define DECLARE_ID(name) const juce::Identifier name (#name);

    DECLARE_ID (PROJECT)
    DECLARE_ID (TRACKS)
    DECLARE_ID (TRACK)
    DECLARE_ID (AUDIO_CLIP)
    DECLARE_ID (MIDI_CLIP)
    DECLARE_ID (name)
    DECLARE_ID (colour)
    DECLARE_ID (volume)
    DECLARE_ID (pan)
    DECLARE_ID (mute)
    DECLARE_ID (solo)
    DECLARE_ID (armed)
    DECLARE_ID (sourceFile)
    DECLARE_ID (startPosition)    // in samples
    DECLARE_ID (length)           // in samples
    DECLARE_ID (trimStart)        // in samples
    DECLARE_ID (trimEnd)          // in samples
    DECLARE_ID (fadeInLength)
    DECLARE_ID (fadeOutLength)
    DECLARE_ID (PLUGIN_CHAIN)
    DECLARE_ID (PLUGIN)
    DECLARE_ID (pluginName)
    DECLARE_ID (pluginFormat)
    DECLARE_ID (pluginManufacturer)
    DECLARE_ID (pluginUniqueId)
    DECLARE_ID (pluginFileOrIdentifier)
    DECLARE_ID (pluginState)
    DECLARE_ID (pluginEnabled)
    DECLARE_ID (tempo)
    DECLARE_ID (timeSigNumerator)
    DECLARE_ID (timeSigDenominator)
    DECLARE_ID (sampleRate)

    // Master bus
    DECLARE_ID (MASTER_BUS)

    // Step sequencer
    DECLARE_ID (STEP_SEQUENCER)
    DECLARE_ID (STEP_PATTERN)
    DECLARE_ID (STEP_ROW)
    DECLARE_ID (STEP)
    DECLARE_ID (numSteps)
    DECLARE_ID (swing)
    DECLARE_ID (activePatternBank)
    DECLARE_ID (activePatternSlot)
    DECLARE_ID (bank)
    DECLARE_ID (slot)
    DECLARE_ID (noteNumber)
    DECLARE_ID (stepDivision)
    DECLARE_ID (index)
    DECLARE_ID (active)
    DECLARE_ID (velocity)
    DECLARE_ID (probability)
    DECLARE_ID (noteLength)

    #undef DECLARE_ID
}

class Project
{
public:
    Project();

    // Serialization (XML)
    bool saveToFile (const juce::File& file) const;
    bool loadFromFile (const juce::File& file);

    // Serialization (YAML session directory)
    bool saveSessionToDirectory (const juce::File& sessionDir) const;
    bool loadSessionFromDirectory (const juce::File& sessionDir);

    // Track management
    juce::ValueTree addTrack (const juce::String& name);
    void removeTrack (int index);
    int getNumTracks() const;
    juce::ValueTree getTrack (int index) const;

    juce::ValueTree& getState() { return state; }
    const juce::ValueTree& getState() const { return state; }

    juce::UndoManager& getUndoManager() { return undoManager; }
    UndoSystem& getUndoSystem() { return undoSystem; }

    Clipboard& getClipboard() { return clipboard; }

    // Master bus state (persistent, holds volume + plugin chain)
    juce::ValueTree getMasterBusState();

    // Project properties
    double getTempo() const;
    void setTempo (double bpm);
    double getSampleRate() const;
    void setSampleRate (double sr);
    int getTimeSigNumerator() const;
    void setTimeSigNumerator (int num);
    int getTimeSigDenominator() const;
    void setTimeSigDenominator (int den);

private:
    juce::ValueTree state;
    juce::UndoManager undoManager;
    UndoSystem undoSystem { undoManager };
    Clipboard clipboard;

    void createDefaultState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};

} // namespace dc
