#pragma once
#include <JuceHeader.h>
#include "Project.h"

namespace dc
{

class StepSequencer
{
public:
    explicit StepSequencer (juce::ValueTree sequencerState);

    juce::ValueTree& getState() { return state; }
    const juce::ValueTree& getState() const { return state; }

    // Global properties
    int getNumSteps() const;
    void setNumSteps (int n, juce::UndoManager* um = nullptr);
    double getSwing() const;
    void setSwing (double s, juce::UndoManager* um = nullptr);
    int getActivePatternBank() const;
    int getActivePatternSlot() const;
    void setActivePattern (int bank, int slot, juce::UndoManager* um = nullptr);

    // Pattern access
    int getNumPatterns() const;
    juce::ValueTree getPattern (int index) const;
    juce::ValueTree getActivePattern() const;

    // Row access (within active pattern)
    int getNumRows() const;
    juce::ValueTree getRow (int rowIndex) const;

    // Step access (within a row)
    static int getStepCount (const juce::ValueTree& row);
    static juce::ValueTree getStep (const juce::ValueTree& row, int stepIndex);

    // Step properties
    static bool isStepActive (const juce::ValueTree& step);
    static int getStepVelocity (const juce::ValueTree& step);
    static double getStepProbability (const juce::ValueTree& step);
    static double getStepNoteLength (const juce::ValueTree& step);

    // Row properties
    static int getRowNoteNumber (const juce::ValueTree& row);
    static juce::String getRowName (const juce::ValueTree& row);
    static bool isRowMuted (const juce::ValueTree& row);
    static bool isRowSoloed (const juce::ValueTree& row);

    // Factory
    static juce::ValueTree createDefaultState();
    static juce::ValueTree createDefaultPattern (int bank, int slot, const juce::String& name, int numSteps);

private:
    juce::ValueTree state;
};

} // namespace dc
