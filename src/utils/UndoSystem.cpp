#include "UndoSystem.h"
#include "dc/foundation/time.h"

namespace dc
{

UndoSystem::UndoSystem (juce::UndoManager& um)
    : undoManager (um)
{
}

void UndoSystem::beginTransaction (const std::string& name)
{
    undoManager.beginNewTransaction (name.c_str());
    currentCoalescingName.clear();
    lastCoalescingTime = 0;
}

void UndoSystem::endTransaction()
{
    // Transactions auto-end when the next one begins, so this is a no-op.
    // Provided for symmetry with beginTransaction.
}

void UndoSystem::beginCoalescedTransaction (const std::string& name, int windowMs)
{
    auto now = dc::currentTimeMillis();

    bool shouldCoalesce = (name == currentCoalescingName)
                          && (lastCoalescingTime != 0)
                          && ((now - lastCoalescingTime) < windowMs);

    if (! shouldCoalesce)
    {
        undoManager.beginNewTransaction (name.c_str());
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

std::string UndoSystem::getUndoDescription() const
{
    return undoManager.getUndoDescription().toStdString();
}

std::string UndoSystem::getRedoDescription() const
{
    return undoManager.getRedoDescription().toStdString();
}

} // namespace dc
