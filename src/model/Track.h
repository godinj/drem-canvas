#pragma once
#include <JuceHeader.h>
#include "Project.h"

namespace dc
{

class Track
{
public:
    explicit Track (const juce::ValueTree& state);

    bool isValid() const { return state.isValid(); }

    juce::String getName() const;
    void setName (const juce::String& name, juce::UndoManager* um = nullptr);

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

    juce::Colour getColour() const;

    // Clip management
    juce::ValueTree addAudioClip (const juce::File& sourceFile, int64_t startPosition, int64_t length);
    int getNumClips() const;
    juce::ValueTree getClip (int index) const;
    void removeClip (int index);

    juce::ValueTree& getState() { return state; }
    const juce::ValueTree& getState() const { return state; }

private:
    juce::ValueTree state;
};

} // namespace dc
