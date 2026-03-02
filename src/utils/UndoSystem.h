#pragma once

#include "dc/model/UndoManager.h"

#include <string>

namespace dc
{

class UndoSystem
{
public:
    UndoSystem();

    // Transaction grouping
    void beginTransaction (const std::string& name = {});
    void endTransaction();

    // Coalescing for continuous edits (e.g. fader drags)
    // Groups rapid edits within a time window into a single undo step
    void beginCoalescedTransaction (const std::string& name, int coalescingWindowMs = 500);

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    std::string getUndoDescription() const;
    std::string getRedoDescription() const;

    UndoManager& getUndoManager() { return undoManager_; }

private:
    UndoManager undoManager_;
    std::string currentCoalescingName_;
    int64_t lastCoalescingTime_ = 0;
    int coalescingWindowMs_ = 500;

    UndoSystem (const UndoSystem&) = delete;
    UndoSystem& operator= (const UndoSystem&) = delete;
};

class ScopedTransaction
{
public:
    explicit ScopedTransaction (UndoSystem& us, const std::string& name)
    {
        us.beginTransaction (name);
    }

    ~ScopedTransaction() = default;

private:
    ScopedTransaction (const ScopedTransaction&) = delete;
    ScopedTransaction& operator= (const ScopedTransaction&) = delete;
};

} // namespace dc
