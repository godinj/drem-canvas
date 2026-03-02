#include "StepSequencer.h"
#include "dc/foundation/assert.h"

namespace dc
{

StepSequencer::StepSequencer (PropertyTree sequencerState)
    : state (sequencerState)
{
    dc_assert (state.getType() == IDs::STEP_SEQUENCER);
}

// --- Global properties ---

int StepSequencer::getNumSteps() const
{
    return static_cast<int> (state.getProperty (IDs::numSteps).getIntOr (16));
}

void StepSequencer::setNumSteps (int n, UndoManager* um)
{
    state.setProperty (IDs::numSteps, Variant (n), um);
}

double StepSequencer::getSwing() const
{
    return state.getProperty (IDs::swing).getDoubleOr (0.0);
}

void StepSequencer::setSwing (double s, UndoManager* um)
{
    state.setProperty (IDs::swing, Variant (s), um);
}

int StepSequencer::getActivePatternBank() const
{
    return static_cast<int> (state.getProperty (IDs::activePatternBank).getIntOr (0));
}

int StepSequencer::getActivePatternSlot() const
{
    return static_cast<int> (state.getProperty (IDs::activePatternSlot).getIntOr (0));
}

void StepSequencer::setActivePattern (int bank, int slotVal, UndoManager* um)
{
    state.setProperty (IDs::activePatternBank, Variant (bank), um);
    state.setProperty (IDs::activePatternSlot, Variant (slotVal), um);
}

// --- Pattern access ---

int StepSequencer::getNumPatterns() const
{
    return state.getNumChildren();
}

PropertyTree StepSequencer::getPattern (int index) const
{
    return state.getChild (index);
}

PropertyTree StepSequencer::getActivePattern() const
{
    int b = getActivePatternBank();
    int s = getActivePatternSlot();

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.getType() == IDs::STEP_PATTERN
            && static_cast<int> (child.getProperty (IDs::bank).getIntOr (-1)) == b
            && static_cast<int> (child.getProperty (IDs::slot).getIntOr (-1)) == s)
            return child;
    }

    return {};
}

// --- Row access ---

int StepSequencer::getNumRows() const
{
    auto pattern = getActivePattern();
    return pattern.isValid() ? pattern.getNumChildren() : 0;
}

PropertyTree StepSequencer::getRow (int rowIndex) const
{
    auto pattern = getActivePattern();
    return pattern.isValid() ? pattern.getChild (rowIndex) : PropertyTree();
}

// --- Step access ---

int StepSequencer::getStepCount (const PropertyTree& row)
{
    return row.getNumChildren();
}

PropertyTree StepSequencer::getStep (const PropertyTree& row, int stepIndex)
{
    return row.getChild (stepIndex);
}

// --- Step properties ---

bool StepSequencer::isStepActive (const PropertyTree& step)
{
    return step.getProperty (IDs::active).getBoolOr (false);
}

int StepSequencer::getStepVelocity (const PropertyTree& step)
{
    return static_cast<int> (step.getProperty (IDs::velocity).getIntOr (100));
}

double StepSequencer::getStepProbability (const PropertyTree& step)
{
    return step.getProperty (IDs::probability).getDoubleOr (1.0);
}

double StepSequencer::getStepNoteLength (const PropertyTree& step)
{
    return step.getProperty (IDs::noteLength).getDoubleOr (1.0);
}

// --- Row properties ---

int StepSequencer::getRowNoteNumber (const PropertyTree& row)
{
    return static_cast<int> (row.getProperty (IDs::noteNumber).getIntOr (36));
}

std::string StepSequencer::getRowName (const PropertyTree& row)
{
    return row.getProperty (IDs::name).getStringOr ("---");
}

bool StepSequencer::isRowMuted (const PropertyTree& row)
{
    return row.getProperty (IDs::mute).getBoolOr (false);
}

bool StepSequencer::isRowSoloed (const PropertyTree& row)
{
    return row.getProperty (IDs::solo).getBoolOr (false);
}

// --- Factory ---

PropertyTree StepSequencer::createDefaultState()
{
    PropertyTree seq (IDs::STEP_SEQUENCER);
    seq.setProperty (IDs::numSteps, Variant (16), nullptr);
    seq.setProperty (IDs::swing, Variant (0.0), nullptr);
    seq.setProperty (IDs::activePatternBank, Variant (0), nullptr);
    seq.setProperty (IDs::activePatternSlot, Variant (0), nullptr);

    seq.addChild (createDefaultPattern (0, 0, "A1", 16), -1, nullptr);

    return seq;
}

PropertyTree StepSequencer::createDefaultPattern (int bankVal, int slotVal,
                                                    const std::string& patternName,
                                                    int numStepsVal)
{
    PropertyTree pattern (IDs::STEP_PATTERN);
    pattern.setProperty (IDs::bank, Variant (bankVal), nullptr);
    pattern.setProperty (IDs::slot, Variant (slotVal), nullptr);
    pattern.setProperty (IDs::name, Variant (patternName), nullptr);
    pattern.setProperty (IDs::numSteps, Variant (numStepsVal), nullptr);
    pattern.setProperty (IDs::stepDivision, Variant (4), nullptr);

    // GM drum map: 8 rows
    struct DrumRow { int note; const char* name; };
    const DrumRow rows[] = {
        { 36, "Kick" },
        { 38, "Snare" },
        { 42, "Closed HH" },
        { 46, "Open HH" },
        { 45, "Low Tom" },
        { 48, "Mid Tom" },
        { 49, "Crash" },
        { 51, "Ride" }
    };

    for (auto& dr : rows)
    {
        PropertyTree row (IDs::STEP_ROW);
        row.setProperty (IDs::noteNumber, Variant (dr.note), nullptr);
        row.setProperty (IDs::name, Variant (std::string (dr.name)), nullptr);
        row.setProperty (IDs::mute, Variant (false), nullptr);
        row.setProperty (IDs::solo, Variant (false), nullptr);

        for (int s = 0; s < numStepsVal; ++s)
        {
            PropertyTree step (IDs::STEP);
            step.setProperty (IDs::index, Variant (s), nullptr);
            step.setProperty (IDs::active, Variant (false), nullptr);
            step.setProperty (IDs::velocity, Variant (100), nullptr);
            step.setProperty (IDs::probability, Variant (1.0), nullptr);
            step.setProperty (IDs::noteLength, Variant (1.0), nullptr);
            row.addChild (step, -1, nullptr);
        }

        pattern.addChild (row, -1, nullptr);
    }

    return pattern;
}

} // namespace dc
