#include "UndoManager.h"

namespace dc {

UndoManager::UndoManager() = default;

void UndoManager::beginTransaction(std::string_view name)
{
    insideTransaction_ = true;
    undoStack_.push_back(Transaction{ std::string(name), {} });

    // Trim oldest transactions if over the limit
    while (static_cast<int>(undoStack_.size()) > maxTransactions_)
        undoStack_.erase(undoStack_.begin());
}

void UndoManager::addAction(std::unique_ptr<UndoAction> action)
{
    // New action after undo clears the redo stack
    if (!redoStack_.empty())
        redoStack_.clear();

    // If no transaction is active, create an implicit one
    if (!insideTransaction_)
        beginTransaction();

    auto& current = undoStack_.back();

    // If the transaction name is empty, use the first action's description
    if (current.name.empty() && current.actions.empty())
        current.name = action->getDescription();

    // Try to merge with the last action in the current transaction
    if (!current.actions.empty() && current.actions.back()->tryMerge(*action))
        return;  // merged — discard the new action

    current.actions.push_back(std::move(action));
}

bool UndoManager::canUndo() const
{
    return !undoStack_.empty();
}

bool UndoManager::canRedo() const
{
    return !redoStack_.empty();
}

bool UndoManager::undo()
{
    if (undoStack_.empty())
        return false;

    insideTransaction_ = false;

    auto transaction = std::move(undoStack_.back());
    undoStack_.pop_back();

    // Reverse all actions in this transaction
    for (auto it = transaction.actions.rbegin(); it != transaction.actions.rend(); ++it)
        (*it)->undo();

    redoStack_.push_back(std::move(transaction));
    return true;
}

bool UndoManager::redo()
{
    if (redoStack_.empty())
        return false;

    insideTransaction_ = false;

    auto transaction = std::move(redoStack_.back());
    redoStack_.pop_back();

    // Re-apply all actions in forward order
    for (auto& action : transaction.actions)
        action->redo();

    undoStack_.push_back(std::move(transaction));
    return true;
}

std::string UndoManager::getUndoDescription() const
{
    if (undoStack_.empty())
        return {};
    return undoStack_.back().name;
}

std::string UndoManager::getRedoDescription() const
{
    if (redoStack_.empty())
        return {};
    return redoStack_.back().name;
}

void UndoManager::clearHistory()
{
    undoStack_.clear();
    redoStack_.clear();
    insideTransaction_ = false;
}

void UndoManager::setMaxTransactions(int max)
{
    maxTransactions_ = max;

    while (static_cast<int>(undoStack_.size()) > maxTransactions_)
        undoStack_.erase(undoStack_.begin());
}

} // namespace dc
