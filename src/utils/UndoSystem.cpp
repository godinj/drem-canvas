#include "UndoSystem.h"

namespace dc
{

UndoSystem::UndoSystem (juce::UndoManager& um)
    : undoManager (um)
{
}

void UndoSystem::beginTransaction (const juce::String& name)
{
    undoManager.beginNewTransaction (name);
    currentCoalescingName.clear();
    lastCoalescingTime = 0;
}

void UndoSystem::endTransaction()
{
    // Transactions auto-end when the next one begins, so this is a no-op.
    // Provided for symmetry with beginTransaction.
}

void UndoSystem::beginCoalescedTransaction (const juce::String& name, int windowMs)
{
    auto now = juce::Time::currentTimeMillis();

    bool shouldCoalesce = (name == currentCoalescingName)
                          && (lastCoalescingTime != 0)
                          && ((now - lastCoalescingTime) < windowMs);

    if (! shouldCoalesce)
    {
        undoManager.beginNewTransaction (name);
        currentCoalescingName = name;
        coalescingWindowMs = windowMs;
    }

    lastCoalescingTime = now;
}

void UndoSystem::undo()
{
    undoManager.undo();
}

void UndoSystem::redo()
{
    undoManager.redo();
}

bool UndoSystem::canUndo() const
{
    return undoManager.canUndo();
}

bool UndoSystem::canRedo() const
{
    return undoManager.canRedo();
}

juce::String UndoSystem::getUndoDescription() const
{
    return undoManager.getUndoDescription();
}

juce::String UndoSystem::getRedoDescription() const
{
    return undoManager.getRedoDescription();
}

} // namespace dc
