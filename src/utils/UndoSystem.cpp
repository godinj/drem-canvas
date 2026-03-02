#include "UndoSystem.h"
#include "dc/foundation/time.h"

namespace dc
{

UndoSystem::UndoSystem() = default;

void UndoSystem::beginTransaction (const std::string& name)
{
    undoManager_.beginTransaction (name);
    currentCoalescingName_.clear();
    lastCoalescingTime_ = 0;
}

void UndoSystem::endTransaction()
{
    // Transactions auto-end when the next one begins, so this is a no-op.
    // Provided for symmetry with beginTransaction.
}

void UndoSystem::beginCoalescedTransaction (const std::string& name, int windowMs)
{
    auto now = dc::currentTimeMillis();

    bool shouldCoalesce = (name == currentCoalescingName_)
                          && (lastCoalescingTime_ != 0)
                          && ((now - lastCoalescingTime_) < windowMs);

    if (! shouldCoalesce)
    {
        undoManager_.beginTransaction (name);
        currentCoalescingName_ = name;
        coalescingWindowMs_ = windowMs;
    }

    lastCoalescingTime_ = now;
}

void UndoSystem::undo()
{
    undoManager_.undo();
}

void UndoSystem::redo()
{
    undoManager_.redo();
}

bool UndoSystem::canUndo() const
{
    return undoManager_.canUndo();
}

bool UndoSystem::canRedo() const
{
    return undoManager_.canRedo();
}

std::string UndoSystem::getUndoDescription() const
{
    return undoManager_.getUndoDescription();
}

std::string UndoSystem::getRedoDescription() const
{
    return undoManager_.getRedoDescription();
}

} // namespace dc
