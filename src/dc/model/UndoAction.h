#pragma once

#include <memory>
#include <string>

namespace dc {

/// Abstract base class for all undoable operations.
class UndoAction
{
public:
    virtual ~UndoAction() = default;

    /// Undo this action (restore previous state)
    virtual void undo() = 0;

    /// Redo this action (re-apply the change)
    virtual void redo() = 0;

    /// Human-readable description for status display
    virtual std::string getDescription() const = 0;

    /// Attempt to merge with a subsequent action of the same type.
    /// Returns true if merged (the other action is then discarded).
    /// Used for coalescing rapid edits (e.g., fader drags).
    virtual bool tryMerge(const UndoAction& /*next*/) { return false; }
};

} // namespace dc
