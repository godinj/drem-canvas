# 07 — Undo System

> Replaces `juce::UndoManager` with a custom command-pattern undo/redo stack
> integrated with `dc::PropertyTree`.

**Phase**: 1 (Model + Undo)
**Dependencies**: Phase 0 (Foundation Types)
**Related**: [01-observable-model.md](01-observable-model.md), [08-migration-guide.md](08-migration-guide.md)

---

## Overview

The current undo system is a thin wrapper (`UndoSystem` in `src/utils/UndoSystem.h`)
around `juce::UndoManager`. It adds:
- Transaction naming (`beginNewTransaction`)
- Coalescing window (groups rapid edits within a time threshold)
- Status display (`getUndoDescription` / `getRedoDescription`)

JUCE's `UndoManager` works by recording `UndoableAction` objects pushed by
ValueTree internally when mutations are made with a non-null undo manager pointer.

The replacement must:
1. Provide an explicit action stack (not hidden inside the tree implementation)
2. Support transactions (group multiple actions into one undo step)
3. Support coalescing (merge rapid successive edits of the same property)
4. Integrate with `dc::PropertyTree` mutations
5. Be message-thread-only (no thread safety needed)

---

## Current Usage

### UndoSystem wrapper (src/utils/UndoSystem.h)

```cpp
class UndoSystem
{
    juce::UndoManager undoManager;
    int64_t lastTransactionTime = 0;
    static constexpr int64_t coalesceWindowMs = 300;

    void beginTransaction(const juce::String& name);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;
    juce::UndoManager& getUndoManager();
};
```

### Call sites

Model mutations pass `&undoManager` to ValueTree:
```cpp
tree.setProperty(IDs::volume, newVolume, &project.getUndoManager());
tree.addChild(newClip, -1, &project.getUndoManager());
track.removeChild(clip, &project.getUndoManager());
```

VimEngine triggers undo/redo:
```cpp
undoSystem.undo();   // 'u' key
undoSystem.redo();   // Ctrl+R
```

---

## Design: `dc::UndoManager`

### dc::UndoAction

Abstract base class for all undoable operations.

```cpp
namespace dc {

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
    virtual bool tryMerge(const UndoAction& next) { return false; }
};

} // namespace dc
```

### Concrete Action Types

These are created internally by `PropertyTree` mutation methods when an
`UndoManager*` is provided.

```cpp
namespace dc {

/// Property set/change
class PropertyChangeAction : public UndoAction
{
public:
    PropertyChangeAction(PropertyTree tree, PropertyId property,
                         Variant oldValue, Variant newValue);
    void undo() override;   // tree.setProperty(prop, oldValue, nullptr)
    void redo() override;   // tree.setProperty(prop, newValue, nullptr)
    std::string getDescription() const override;
    bool tryMerge(const UndoAction& next) override;
    // Merges if same tree + same property (keeps original oldValue, takes new newValue)
};

/// Child added
class ChildAddAction : public UndoAction
{
public:
    ChildAddAction(PropertyTree parent, PropertyTree child, int index);
    void undo() override;   // parent.removeChild(index, nullptr)
    void redo() override;   // parent.addChild(child, index, nullptr)
    std::string getDescription() const override;
};

/// Child removed
class ChildRemoveAction : public UndoAction
{
public:
    ChildRemoveAction(PropertyTree parent, PropertyTree child, int index);
    void undo() override;   // parent.addChild(child, index, nullptr)
    void redo() override;   // parent.removeChild(index, nullptr)
    std::string getDescription() const override;
};

/// Child order changed
class ChildMoveAction : public UndoAction
{
public:
    ChildMoveAction(PropertyTree parent, int oldIndex, int newIndex);
    void undo() override;   // parent.moveChild(newIndex, oldIndex, nullptr)
    void redo() override;   // parent.moveChild(oldIndex, newIndex, nullptr)
    std::string getDescription() const override;
};

} // namespace dc
```

### dc::UndoManager

```cpp
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
    /// If coalescing is enabled and the action can merge with the
    /// previous action in the current transaction, they are merged.
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
```

### Transaction Semantics

1. `beginTransaction("Set volume")` opens a new group
2. Subsequent `addAction()` calls append to the current group
3. `undo()` reverses all actions in the most recent group (in reverse order)
4. `redo()` re-applies all actions in the group (in forward order)
5. Any new action after an undo clears the redo stack

If `addAction()` is called without `beginTransaction()`, a new implicit
transaction is created automatically (matching JUCE behavior).

---

## Coalescing

Coalescing merges rapid successive edits of the same property into a single
undo step. This prevents "undo" from stepping through every intermediate
fader position during a drag.

### Mechanism

The `UndoSystem` wrapper (or the new equivalent) manages coalescing at the
transaction level:

```cpp
namespace dc {

class UndoSystem
{
public:
    UndoSystem();

    /// Begin a transaction with coalescing support.
    /// If called within coalesceWindowMs of the previous transaction
    /// with the same name, the previous transaction is extended instead
    /// of creating a new one.
    void beginTransaction(std::string_view name = {});

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    std::string getUndoDescription() const;
    std::string getRedoDescription() const;

    /// Access the underlying UndoManager (for passing to PropertyTree)
    UndoManager& getUndoManager();

    /// Coalescing window in milliseconds (default: 300ms)
    void setCoalesceWindow(int64_t ms);

private:
    UndoManager undoManager_;
    int64_t lastTransactionTimeMs_ = 0;
    std::string lastTransactionName_;
    int64_t coalesceWindowMs_ = 300;
};

} // namespace dc
```

### Coalescing Flow

```
t=0ms:   beginTransaction("Set volume")   → new transaction
t=5ms:   addAction(PropertyChange vol 0→0.5)
t=50ms:  beginTransaction("Set volume")   → same name within 300ms, extend
t=55ms:  addAction(PropertyChange vol 0.5→0.6)  → tryMerge succeeds: 0→0.6
t=100ms: beginTransaction("Set volume")   → still within 300ms, extend
t=105ms: addAction(PropertyChange vol 0.6→0.8)  → tryMerge succeeds: 0→0.8
t=500ms: beginTransaction("Set pan")      → different name, new transaction
```

Result: One undo step "Set volume" (0→0.8), one undo step "Set pan".

### tryMerge for PropertyChangeAction

```cpp
bool PropertyChangeAction::tryMerge(const UndoAction& next)
{
    auto* other = dynamic_cast<const PropertyChangeAction*>(&next);
    if (!other) return false;
    if (tree_ != other->tree_) return false;
    if (property_ != other->property_) return false;

    // Keep our oldValue, take their newValue
    newValue_ = other->newValue_;
    return true;
}
```

---

## Integration with PropertyTree

PropertyTree mutation methods create actions internally:

```cpp
void PropertyTree::setProperty(PropertyId name, Variant value,
                               UndoManager* undoManager)
{
    if (undoManager)
    {
        Variant oldValue = getProperty(name);
        undoManager->addAction(
            std::make_unique<PropertyChangeAction>(
                *this, name, std::move(oldValue), value));
    }

    // Apply the change
    data_->setPropertyInternal(name, std::move(value));

    // Notify listeners
    data_->notifyPropertyChanged(name);
}
```

When `undo()` is called, the action calls `tree.setProperty(prop, oldValue, nullptr)` —
passing `nullptr` for the undo manager to avoid recording undo-of-undo.

---

## Migration from Current UndoSystem

| Current (`src/utils/UndoSystem.h`) | New (`dc::UndoSystem`) |
|-------------------------------------|------------------------|
| `juce::UndoManager undoManager` | `dc::UndoManager undoManager_` |
| `undoManager.beginNewTransaction(name)` | `undoManager_.beginTransaction(name)` |
| `undoManager.undo()` | `undoManager_.undo()` |
| `undoManager.redo()` | `undoManager_.redo()` |
| `undoManager.canUndo()` | `undoManager_.canUndo()` |
| `undoManager.getUndoDescription()` | `undoManager_.getUndoDescription()` |
| `juce::Time::currentTimeMillis()` | `dc::currentTimeMillis()` or `std::chrono` |

The call-site pattern is identical:
```cpp
// Before
tree.setProperty(IDs::volume, 0.8, &project.getUndoManager());
// After
tree.setProperty(IDs::volume, dc::Variant(0.8), &project.getUndoManager());
```

---

## Files Affected

| File | Changes |
|------|---------|
| `src/utils/UndoSystem.h/.cpp` | Rewrite internals to use dc::UndoManager |
| `src/model/Project.h/.cpp` | `juce::UndoManager` → `dc::UndoManager` in getUndoManager() |
| All model mutation sites | `&undoManager` parameter unchanged (type changes) |
| `src/vim/VimEngine.cpp` | `undo()` / `redo()` calls unchanged |
| `src/ui/vim/VimStatusBarWidget.cpp` | `getUndoDescription()` unchanged |

## Testing Strategy

1. **Unit test**: Push 5 actions, undo 3, redo 2, verify state at each step
2. **Coalescing test**: Rapid property changes within window → single undo step
3. **Transaction test**: Multiple actions in one transaction undo/redo as group
4. **Integration test**: PropertyTree mutations with undo → full round-trip
5. **Edge cases**: Undo with empty stack, redo after new action (clears redo stack),
   max transaction limit
