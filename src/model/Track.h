#pragma once
#include <JuceHeader.h>
#include <string>
#include <filesystem>
#include "dc/foundation/types.h"
#include "Project.h"

namespace dc
{

class Track
{
public:
    explicit Track (const juce::ValueTree& state);

    bool isValid() const { return state.isValid(); }

    std::string getName() const;
    void setName (const std::string& name, juce::UndoManager* um = nullptr);

    float getVolume() const;
    void setVolume (float vol, juce::UndoManager* um = nullptr);

    float getPan() const;
    void setPan (float p, juce::UndoManager* um = nullptr);

    bool isMuted() const;
    void setMuted (bool m, juce::UndoManager* um = nullptr);

    bool isSolo() const;
    void setSolo (bool s, juce::UndoManager* um = nullptr);

    bool isArmed() const;
    void setArmed (bool a, juce::UndoManager* um = nullptr);

    dc::Colour getColour() const;

    // Clip management
    juce::ValueTree addAudioClip (const std::filesystem::path& sourceFile, int64_t startPosition, int64_t length);
    juce::ValueTree addMidiClip (int64_t startPosition, int64_t length);
    int getNumClips() const;
    juce::ValueTree getClip (int index) const;
    void removeClip (int index, juce::UndoManager* um = nullptr);

    // Plugin chain management
    juce::ValueTree getPluginChain();
    juce::ValueTree addPlugin (const std::string& name, const std::string& format,
                               const std::string& manufacturer, int uniqueId,
                               const std::string& fileOrIdentifier,
                               juce::UndoManager* um = nullptr);
    void removePlugin (int index, juce::UndoManager* um = nullptr);
    void movePlugin (int fromIndex, int toIndex, juce::UndoManager* um = nullptr);
    int getNumPlugins() const;
    juce::ValueTree getPlugin (int index) const;
    void setPluginEnabled (int index, bool enabled, juce::UndoManager* um = nullptr);
    bool isPluginEnabled (int index) const;
    void setPluginState (int index, const std::string& base64State, juce::UndoManager* um = nullptr);

    juce::ValueTree& getState() { return state; }
    const juce::ValueTree& getState() const { return state; }

private:
    juce::ValueTree state;
};

} // namespace dc
