#pragma once
#include "dc/model/PropertyTree.h"
#include "dc/model/PropertyId.h"
#include "dc/model/Variant.h"
#include <string>
#include <filesystem>
#include "utils/UndoSystem.h"
#include "Clipboard.h"

namespace dc
{

namespace IDs
{
    #define DECLARE_ID(name) const dc::PropertyId name (#name);

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
    DECLARE_ID (cycleEnabled)
    DECLARE_ID (cycleStart)     // in samples
    DECLARE_ID (cycleEnd)       // in samples

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

    // MIDI note children
    DECLARE_ID (NOTE)
    DECLARE_ID (startBeat)
    DECLARE_ID (lengthBeats)

    // CC automation children
    DECLARE_ID (CC_POINT)
    DECLARE_ID (ccNumber)
    DECLARE_ID (beat)
    DECLARE_ID (value)

    // Step sequencer row labels
    DECLARE_ID (label)

    #undef DECLARE_ID
}

class Project
{
public:
    Project();

    // Serialization (XML)
    bool saveToFile (const std::filesystem::path& file) const;
    bool loadFromFile (const std::filesystem::path& file);

    // Serialization (YAML session directory)
    bool saveSessionToDirectory (const std::filesystem::path& sessionDir) const;
    bool loadSessionFromDirectory (const std::filesystem::path& sessionDir);

    // Track management
    PropertyTree addTrack (const std::string& name);
    void removeTrack (int index);
    int getNumTracks() const;
    PropertyTree getTrack (int index) const;

    PropertyTree& getState() { return state; }
    const PropertyTree& getState() const { return state; }

    UndoManager& getUndoManager() { return undoSystem.getUndoManager(); }
    UndoSystem& getUndoSystem() { return undoSystem; }

    Clipboard& getClipboard() { return clipboard; }

    // Master bus state (persistent, holds volume + plugin chain)
    PropertyTree getMasterBusState();

    // Project properties
    double getTempo() const;
    void setTempo (double bpm);
    double getSampleRate() const;
    void setSampleRate (double sr);
    int getTimeSigNumerator() const;
    void setTimeSigNumerator (int num);
    int getTimeSigDenominator() const;
    void setTimeSigDenominator (int den);

    bool getCycleEnabled() const;
    void setCycleEnabled (bool enabled);
    int64_t getCycleStart() const;
    void setCycleStart (int64_t startInSamples);
    int64_t getCycleEnd() const;
    void setCycleEnd (int64_t endInSamples);

private:
    PropertyTree state;
    UndoSystem undoSystem;
    Clipboard clipboard;

    void createDefaultState();

    Project (const Project&) = delete;
    Project& operator= (const Project&) = delete;
};

} // namespace dc
