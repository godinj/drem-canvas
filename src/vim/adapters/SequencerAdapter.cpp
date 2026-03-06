#include "vim/adapters/SequencerAdapter.h"
#include "vim/ActionRegistry.h"
#include "model/StepSequencer.h"
#include "utils/UndoSystem.h"
#include "dc/foundation/time.h"
#include <algorithm>

namespace dc
{

static bool isEscapeOrCtrlC (const dc::KeyPress& key)
{
    if (key == dc::KeyCode::Escape)
        return true;

    if (key.control)
    {
        auto c = key.getTextCharacter();
        if (c == 3 || c == 'c' || c == 'C')
            return true;
    }

    return false;
}

SequencerAdapter::SequencerAdapter (Project& p, VimContext& ctx)
    : project (p), context (ctx)
{
}

bool SequencerAdapter::handleRawKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape / Ctrl-C
    if (isEscapeOrCtrlC (key))
    {
        if (onContextChanged) onContextChanged();
        return true;
    }

    // Pending 'g' for gg
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (dc::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            pendingKey = 0;
            pendingTimestamp = 0;
            jumpFirstRow();
            return true;
        }
        pendingKey = 0;
        pendingTimestamp = 0;
    }

    // Navigation
    if (keyChar == 'h') { moveLeft();  return true; }
    if (keyChar == 'l') { moveRight(); return true; }
    if (keyChar == 'j') { moveDown();  return true; }
    if (keyChar == 'k') { moveUp();    return true; }

    // Jump keys
    if (keyChar == '0') { jumpFirstStep(); return true; }
    if (keyChar == '$') { jumpLastStep();  return true; }
    if (keyChar == 'G') { jumpLastRow();   return true; }
    if (keyChar == 'g')
    {
        pendingKey = 'g';
        pendingTimestamp = dc::currentTimeMillis();
        if (onContextChanged) onContextChanged();
        return true;
    }

    // Toggle step
    if (key == dc::KeyCode::Space)
    {
        toggleStep();
        return true;
    }

    // Velocity adjust
    if (keyChar == '+' || keyChar == '=') { adjustVelocity (10);  return true; }
    if (keyChar == '-')                   { adjustVelocity (-10); return true; }
    if (keyChar == 'v')                   { cycleVelocity();      return true; }

    // Row mute/solo
    if (keyChar == 'M') { toggleRowMute(); return true; }
    if (keyChar == 'S') { toggleRowSolo(); return true; }

    return false;
}

// ── Navigation ──────────────────────────────────────────────────────────────

void SequencerAdapter::moveLeft()
{
    int step = context.getSeqStep();
    if (step > 0)
    {
        context.setSeqStep (step - 1);
        if (onContextChanged) onContextChanged();
    }
}

void SequencerAdapter::moveRight()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int maxStep = 0;
    auto pattern = seq.getActivePattern();
    if (pattern.isValid())
        maxStep = static_cast<int> (pattern.getProperty (IDs::numSteps).getIntOr (16)) - 1;

    int step = context.getSeqStep();
    if (step < maxStep)
    {
        context.setSeqStep (step + 1);
        if (onContextChanged) onContextChanged();
    }
}

void SequencerAdapter::moveUp()
{
    int row = context.getSeqRow();
    if (row > 0)
    {
        context.setSeqRow (row - 1);
        if (onContextChanged) onContextChanged();
    }
}

void SequencerAdapter::moveDown()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int maxRow = seq.getNumRows() - 1;

    int row = context.getSeqRow();
    if (row < maxRow)
    {
        context.setSeqRow (row + 1);
        if (onContextChanged) onContextChanged();
    }
}

void SequencerAdapter::jumpFirstStep()
{
    context.setSeqStep (0);
    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::jumpLastStep()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto pattern = seq.getActivePattern();
    if (! pattern.isValid()) return;

    int lastStep = static_cast<int> (pattern.getProperty (IDs::numSteps).getIntOr (16)) - 1;
    context.setSeqStep (std::max (0, lastStep));
    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::jumpFirstRow()
{
    context.setSeqRow (0);
    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::jumpLastRow()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int lastRow = seq.getNumRows() - 1;
    context.setSeqRow (std::max (0, lastRow));
    if (onContextChanged) onContextChanged();
}

// ── Editing ─────────────────────────────────────────────────────────────────

void SequencerAdapter::toggleStep()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Step");
    bool isActive = StepSequencer::isStepActive (step);
    step.setProperty (IDs::active, ! isActive, &project.getUndoManager());

    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::adjustVelocity (int delta)
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    int vel = StepSequencer::getStepVelocity (step) + delta;
    vel = std::clamp (vel, 1, 127);

    ScopedTransaction txn (project.getUndoSystem(), "Adjust Velocity");
    step.setProperty (IDs::velocity, vel, &project.getUndoManager());

    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::cycleVelocity()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    // Cycle through preset velocities
    const int presets[] = { 25, 50, 75, 100, 127 };
    const int numPresets = 5;
    int currentVel = StepSequencer::getStepVelocity (step);

    int nextIdx = 0;
    for (int i = 0; i < numPresets; ++i)
    {
        if (presets[i] > currentVel)
        {
            nextIdx = i;
            break;
        }
        if (i == numPresets - 1)
            nextIdx = 0; // wrap around
    }

    ScopedTransaction txn (project.getUndoSystem(), "Cycle Velocity");
    step.setProperty (IDs::velocity, presets[nextIdx], &project.getUndoManager());

    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::toggleRowMute()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Row Mute");
    bool muted = StepSequencer::isRowMuted (row);
    row.setProperty (IDs::mute, ! muted, &project.getUndoManager());

    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::toggleRowSolo()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Row Solo");
    bool soloed = StepSequencer::isRowSoloed (row);
    row.setProperty (IDs::solo, ! soloed, &project.getUndoManager());

    if (onContextChanged) onContextChanged();
}

void SequencerAdapter::registerActions (ActionRegistry& registry)
{
    // Sequencer-specific actions can be registered here in the future
}

} // namespace dc
