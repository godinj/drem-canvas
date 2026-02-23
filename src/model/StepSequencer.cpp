#include "StepSequencer.h"

namespace dc
{

StepSequencer::StepSequencer (juce::ValueTree sequencerState)
    : state (sequencerState)
{
    jassert (state.hasType (IDs::STEP_SEQUENCER));
}

// --- Global properties ---

int StepSequencer::getNumSteps() const
{
    return state.getProperty (IDs::numSteps, 16);
}

void StepSequencer::setNumSteps (int n, juce::UndoManager* um)
{
    state.setProperty (IDs::numSteps, n, um);
}

double StepSequencer::getSwing() const
{
    return state.getProperty (IDs::swing, 0.0);
}

void StepSequencer::setSwing (double s, juce::UndoManager* um)
{
    state.setProperty (IDs::swing, s, um);
}

int StepSequencer::getActivePatternBank() const
{
    return state.getProperty (IDs::activePatternBank, 0);
}

int StepSequencer::getActivePatternSlot() const
{
    return state.getProperty (IDs::activePatternSlot, 0);
}

void StepSequencer::setActivePattern (int bank, int slotVal, juce::UndoManager* um)
{
    state.setProperty (IDs::activePatternBank, bank, um);
    state.setProperty (IDs::activePatternSlot, slotVal, um);
}

// --- Pattern access ---

int StepSequencer::getNumPatterns() const
{
    return state.getNumChildren();
}

juce::ValueTree StepSequencer::getPattern (int index) const
{
    return state.getChild (index);
}

juce::ValueTree StepSequencer::getActivePattern() const
{
    int b = getActivePatternBank();
    int s = getActivePatternSlot();

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.hasType (IDs::STEP_PATTERN)
            && static_cast<int> (child.getProperty (IDs::bank, -1)) == b
            && static_cast<int> (child.getProperty (IDs::slot, -1)) == s)
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

juce::ValueTree StepSequencer::getRow (int rowIndex) const
{
    auto pattern = getActivePattern();
    return pattern.isValid() ? pattern.getChild (rowIndex) : juce::ValueTree();
}

// --- Step access ---

int StepSequencer::getStepCount (const juce::ValueTree& row)
{
    return row.getNumChildren();
}

juce::ValueTree StepSequencer::getStep (const juce::ValueTree& row, int stepIndex)
{
    return row.getChild (stepIndex);
}

// --- Step properties ---

bool StepSequencer::isStepActive (const juce::ValueTree& step)
{
    return step.getProperty (IDs::active, false);
}

int StepSequencer::getStepVelocity (const juce::ValueTree& step)
{
    return step.getProperty (IDs::velocity, 100);
}

double StepSequencer::getStepProbability (const juce::ValueTree& step)
{
    return step.getProperty (IDs::probability, 1.0);
}

double StepSequencer::getStepNoteLength (const juce::ValueTree& step)
{
    return step.getProperty (IDs::noteLength, 1.0);
}

// --- Row properties ---

int StepSequencer::getRowNoteNumber (const juce::ValueTree& row)
{
    return row.getProperty (IDs::noteNumber, 36);
}

juce::String StepSequencer::getRowName (const juce::ValueTree& row)
{
    return row.getProperty (IDs::name, "---");
}

bool StepSequencer::isRowMuted (const juce::ValueTree& row)
{
    return row.getProperty (IDs::mute, false);
}

bool StepSequencer::isRowSoloed (const juce::ValueTree& row)
{
    return row.getProperty (IDs::solo, false);
}

// --- Factory ---

juce::ValueTree StepSequencer::createDefaultState()
{
    juce::ValueTree seq (IDs::STEP_SEQUENCER);
    seq.setProperty (IDs::numSteps, 16, nullptr);
    seq.setProperty (IDs::swing, 0.0, nullptr);
    seq.setProperty (IDs::activePatternBank, 0, nullptr);
    seq.setProperty (IDs::activePatternSlot, 0, nullptr);

    seq.appendChild (createDefaultPattern (0, 0, "A1", 16), nullptr);

    return seq;
}

juce::ValueTree StepSequencer::createDefaultPattern (int bankVal, int slotVal,
                                                      const juce::String& patternName,
                                                      int numStepsVal)
{
    juce::ValueTree pattern (IDs::STEP_PATTERN);
    pattern.setProperty (IDs::bank, bankVal, nullptr);
    pattern.setProperty (IDs::slot, slotVal, nullptr);
    pattern.setProperty (IDs::name, patternName, nullptr);
    pattern.setProperty (IDs::numSteps, numStepsVal, nullptr);
    pattern.setProperty (IDs::stepDivision, 4, nullptr);

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
        juce::ValueTree row (IDs::STEP_ROW);
        row.setProperty (IDs::noteNumber, dr.note, nullptr);
        row.setProperty (IDs::name, juce::String (dr.name), nullptr);
        row.setProperty (IDs::mute, false, nullptr);
        row.setProperty (IDs::solo, false, nullptr);

        for (int s = 0; s < numStepsVal; ++s)
        {
            juce::ValueTree step (IDs::STEP);
            step.setProperty (IDs::index, s, nullptr);
            step.setProperty (IDs::active, false, nullptr);
            step.setProperty (IDs::velocity, 100, nullptr);
            step.setProperty (IDs::probability, 1.0, nullptr);
            step.setProperty (IDs::noteLength, 1.0, nullptr);
            row.appendChild (step, nullptr);
        }

        pattern.appendChild (row, nullptr);
    }

    return pattern;
}

} // namespace dc
