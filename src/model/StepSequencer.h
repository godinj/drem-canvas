#pragma once
#include <string>
#include "Project.h"

namespace dc
{

class StepSequencer
{
public:
    explicit StepSequencer (PropertyTree sequencerState);

    PropertyTree& getState() { return state; }
    const PropertyTree& getState() const { return state; }

    // Global properties
    int getNumSteps() const;
    void setNumSteps (int n, UndoManager* um = nullptr);
    double getSwing() const;
    void setSwing (double s, UndoManager* um = nullptr);
    int getActivePatternBank() const;
    int getActivePatternSlot() const;
    void setActivePattern (int bank, int slot, UndoManager* um = nullptr);

    // Pattern access
    int getNumPatterns() const;
    PropertyTree getPattern (int index) const;
    PropertyTree getActivePattern() const;

    // Row access (within active pattern)
    int getNumRows() const;
    PropertyTree getRow (int rowIndex) const;

    // Step access (within a row)
    static int getStepCount (const PropertyTree& row);
    static PropertyTree getStep (const PropertyTree& row, int stepIndex);

    // Step properties
    static bool isStepActive (const PropertyTree& step);
    static int getStepVelocity (const PropertyTree& step);
    static double getStepProbability (const PropertyTree& step);
    static double getStepNoteLength (const PropertyTree& step);

    // Row properties
    static int getRowNoteNumber (const PropertyTree& row);
    static std::string getRowName (const PropertyTree& row);
    static bool isRowMuted (const PropertyTree& row);
    static bool isRowSoloed (const PropertyTree& row);

    // Factory
    static PropertyTree createDefaultState();
    static PropertyTree createDefaultPattern (int bank, int slot, const std::string& name, int numSteps);

private:
    PropertyTree state;
};

} // namespace dc
