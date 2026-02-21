#pragma once
#include <JuceHeader.h>
#include "Project.h"

namespace dc
{

class AudioClip
{
public:
    explicit AudioClip (const juce::ValueTree& state);

    bool isValid() const { return state.isValid(); }

    juce::File getSourceFile() const;
    int64_t getStartPosition() const;
    void setStartPosition (int64_t pos, juce::UndoManager* um = nullptr);
    int64_t getLength() const;
    int64_t getTrimStart() const;
    int64_t getTrimEnd() const;
    int64_t getFadeInLength() const;
    int64_t getFadeOutLength() const;

    juce::ValueTree& getState() { return state; }

private:
    juce::ValueTree state;
};

} // namespace dc
