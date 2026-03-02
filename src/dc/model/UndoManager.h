#pragma once

#include "UndoAction.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dc {

class UndoManager
{
public:
    UndoManager();

    // --- Transaction control ---

    /// Begin a new transaction. All subsequent actions until the next
    /// beginTransaction() are grouped as one undo step.
    /// If name is empty, uses the first action's description.
    void beginTransaction(std::string_view name = {});

    // --- Adding actions ---

    /// Add an action to the current transaction.
    /// If no transaction is active, creates an implicit one.
    /// If the action can merge with the previous action in the
    /// current transaction, they are merged.
    void addAction(std::unique_ptr<UndoAction> action);

    // --- Undo/Redo ---

    bool canUndo() const;
    bool canRedo() const;

    /// Undo the most recent transaction (all actions in reverse order).
    /// Returns false if nothing to undo.
    bool undo();

    /// Redo the most recently undone transaction.
    /// Returns false if nothing to redo.
    bool redo();

    // --- Status ---

    std::string getUndoDescription() const;
    std::string getRedoDescription() const;

    // --- History management ---

    /// Clear all undo/redo history
    void clearHistory();

    /// Maximum number of transactions to keep (default: 100)
    void setMaxTransactions(int max);

private:
    struct Transaction
    {
        std::string name;
        std::vector<std::unique_ptr<UndoAction>> actions;
    };

    std::vector<Transaction> undoStack_;
    std::vector<Transaction> redoStack_;
    int maxTransactions_ = 100;
    bool insideTransaction_ = false;
};

} // namespace dc
