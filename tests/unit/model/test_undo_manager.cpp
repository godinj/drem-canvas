#include <catch2/catch_test_macros.hpp>
#include <dc/model/UndoManager.h>
#include <dc/model/UndoAction.h>

#include <memory>
#include <string>
#include <vector>

using dc::UndoAction;
using dc::UndoManager;

namespace {

/// A simple test action that toggles an int between old and new values.
class TestAction : public UndoAction
{
public:
    TestAction (int& target, int oldVal, int newVal,
                std::string desc = "TestAction")
        : target_ (target)
        , oldValue_ (oldVal)
        , newValue_ (newVal)
        , description_ (std::move (desc))
    {
    }

    void undo() override { target_ = oldValue_; }
    void redo() override { target_ = newValue_; }
    std::string getDescription() const override { return description_; }

private:
    int& target_;
    int oldValue_;
    int newValue_;
    std::string description_;
};

/// A mergeable action for testing coalescing.
class MergeableAction : public UndoAction
{
public:
    MergeableAction (int& target, int oldVal, int newVal, int key)
        : target_ (target)
        , oldValue_ (oldVal)
        , newValue_ (newVal)
        , key_ (key)
    {
    }

    void undo() override { target_ = oldValue_; }
    void redo() override { target_ = newValue_; }
    std::string getDescription() const override { return "Mergeable"; }

    bool tryMerge (const UndoAction& next) override
    {
        auto* other = dynamic_cast<const MergeableAction*> (&next);
        if (other && other->key_ == key_)
        {
            newValue_ = other->newValue_;
            return true;
        }
        return false;
    }

private:
    int& target_;
    int oldValue_;
    int newValue_;
    int key_;
};

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
// Single action: undo / redo
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: single action undo restores state", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("set to 42");
    undo.addAction (std::make_unique<TestAction> (value, 0, 42));
    value = 42;

    REQUIRE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());

    undo.undo();
    REQUIRE (value == 0);
}

TEST_CASE ("UndoManager: single action redo re-applies state", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("set to 42");
    undo.addAction (std::make_unique<TestAction> (value, 0, 42));
    value = 42;

    undo.undo();
    REQUIRE (value == 0);
    REQUIRE (undo.canRedo());

    undo.redo();
    REQUIRE (value == 42);
}

// ═══════════════════════════════════════════════════════════════
// Transaction grouping
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: transaction groups multiple actions into one undo step", "[model][undo]")
{
    UndoManager undo;
    int a = 0;
    int b = 0;

    undo.beginTransaction ("batch edit");
    undo.addAction (std::make_unique<TestAction> (a, 0, 10, "set a"));
    a = 10;
    undo.addAction (std::make_unique<TestAction> (b, 0, 20, "set b"));
    b = 20;

    REQUIRE (a == 10);
    REQUIRE (b == 20);

    // Single undo should revert both actions
    undo.undo();

    REQUIRE (a == 0);
    REQUIRE (b == 0);
}

TEST_CASE ("UndoManager: multiple transactions undo independently", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("first");
    undo.addAction (std::make_unique<TestAction> (value, 0, 1));
    value = 1;

    undo.beginTransaction ("second");
    undo.addAction (std::make_unique<TestAction> (value, 1, 2));
    value = 2;

    undo.undo();  // undo "second"
    REQUIRE (value == 1);

    undo.undo();  // undo "first"
    REQUIRE (value == 0);
}

// ═══════════════════════════════════════════════════════════════
// Redo stack cleared on new action
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: redo stack cleared when new action is added after undo", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("set to 1");
    undo.addAction (std::make_unique<TestAction> (value, 0, 1));
    value = 1;

    undo.undo();
    REQUIRE (undo.canRedo());

    // Add a new action — should clear redo
    undo.beginTransaction ("set to 99");
    undo.addAction (std::make_unique<TestAction> (value, 0, 99));
    value = 99;

    REQUIRE_FALSE (undo.canRedo());
}

// ═══════════════════════════════════════════════════════════════
// Action coalescing via tryMerge()
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: same-key actions merge within a transaction", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("drag");

    // Simulate a fader drag: multiple incremental changes with the same key
    undo.addAction (std::make_unique<MergeableAction> (value, 0, 10, /*key=*/1));
    undo.addAction (std::make_unique<MergeableAction> (value, 10, 20, /*key=*/1));
    undo.addAction (std::make_unique<MergeableAction> (value, 20, 30, /*key=*/1));
    value = 30;

    // All three should have merged into one action (oldValue=0, newValue=30)
    undo.undo();
    REQUIRE (value == 0);  // reverted to original value
}

TEST_CASE ("UndoManager: different-key actions do not merge", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("edits");

    undo.addAction (std::make_unique<MergeableAction> (value, 0, 10, /*key=*/1));
    undo.addAction (std::make_unique<MergeableAction> (value, 10, 20, /*key=*/2));  // different key
    value = 20;

    // Two separate actions in the transaction; undo reverses both
    undo.undo();
    REQUIRE (value == 0);
}

// ═══════════════════════════════════════════════════════════════
// canUndo / canRedo states
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: canUndo / canRedo on empty manager", "[model][undo]")
{
    UndoManager undo;
    REQUIRE_FALSE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
}

TEST_CASE ("UndoManager: undo on empty stack returns false", "[model][undo]")
{
    UndoManager undo;
    REQUIRE_FALSE (undo.undo());
}

TEST_CASE ("UndoManager: redo on empty stack returns false", "[model][undo]")
{
    UndoManager undo;
    REQUIRE_FALSE (undo.redo());
}

// ═══════════════════════════════════════════════════════════════
// clearHistory
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: clearHistory empties both stacks", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("first");
    undo.addAction (std::make_unique<TestAction> (value, 0, 1));
    value = 1;

    undo.undo();

    REQUIRE (undo.canRedo());

    undo.clearHistory();

    REQUIRE_FALSE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
}

// ═══════════════════════════════════════════════════════════════
// setMaxTransactions
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: setMaxTransactions trims oldest transactions", "[model][undo]")
{
    UndoManager undo;
    undo.setMaxTransactions (2);

    int a = 0, b = 0, c = 0;

    undo.beginTransaction ("set a");
    undo.addAction (std::make_unique<TestAction> (a, 0, 1));
    a = 1;

    undo.beginTransaction ("set b");
    undo.addAction (std::make_unique<TestAction> (b, 0, 2));
    b = 2;

    undo.beginTransaction ("set c");
    undo.addAction (std::make_unique<TestAction> (c, 0, 3));
    c = 3;

    // Only 2 transactions should remain (b and c), "set a" was trimmed
    REQUIRE (undo.canUndo());

    undo.undo();  // undo "set c"
    REQUIRE (c == 0);

    undo.undo();  // undo "set b"
    REQUIRE (b == 0);

    // "set a" was trimmed, so no more undo
    REQUIRE_FALSE (undo.canUndo());
}

// ═══════════════════════════════════════════════════════════════
// Implicit transaction
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: addAction without beginTransaction creates implicit transaction", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.addAction (std::make_unique<TestAction> (value, 0, 42, "implicit"));
    value = 42;

    REQUIRE (undo.canUndo());

    undo.undo();
    REQUIRE (value == 0);
}

// ═══════════════════════════════════════════════════════════════
// Multiple beginTransaction calls
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: consecutive beginTransaction calls create separate transactions", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("first");
    undo.addAction (std::make_unique<TestAction> (value, 0, 1));
    value = 1;

    undo.beginTransaction ("second");
    undo.addAction (std::make_unique<TestAction> (value, 1, 2));
    value = 2;

    undo.undo();  // undo "second"
    REQUIRE (value == 1);

    undo.undo();  // undo "first"
    REQUIRE (value == 0);
}

// ═══════════════════════════════════════════════════════════════
// Descriptions
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: getUndoDescription returns transaction name", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("my edit");
    undo.addAction (std::make_unique<TestAction> (value, 0, 1, "ignored"));
    value = 1;

    REQUIRE (undo.getUndoDescription() == "my edit");
}

TEST_CASE ("UndoManager: getUndoDescription uses first action description when name is empty", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ();  // empty name
    undo.addAction (std::make_unique<TestAction> (value, 0, 1, "auto-name"));
    value = 1;

    REQUIRE (undo.getUndoDescription() == "auto-name");
}

TEST_CASE ("UndoManager: getRedoDescription after undo", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    undo.beginTransaction ("my edit");
    undo.addAction (std::make_unique<TestAction> (value, 0, 1));
    value = 1;

    undo.undo();

    REQUIRE (undo.getRedoDescription() == "my edit");
}

TEST_CASE ("UndoManager: getUndoDescription on empty stack returns empty string", "[model][undo]")
{
    UndoManager undo;
    REQUIRE (undo.getUndoDescription().empty());
}

TEST_CASE ("UndoManager: getRedoDescription on empty stack returns empty string", "[model][undo]")
{
    UndoManager undo;
    REQUIRE (undo.getRedoDescription().empty());
}

// ═══════════════════════════════════════════════════════════════
// Complex undo/redo scenario
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("UndoManager: complex undo/redo sequence", "[model][undo]")
{
    UndoManager undo;
    int value = 0;

    // Transaction 1: 0 -> 10
    undo.beginTransaction ("t1");
    undo.addAction (std::make_unique<TestAction> (value, 0, 10));
    value = 10;

    // Transaction 2: 10 -> 20
    undo.beginTransaction ("t2");
    undo.addAction (std::make_unique<TestAction> (value, 10, 20));
    value = 20;

    // Transaction 3: 20 -> 30
    undo.beginTransaction ("t3");
    undo.addAction (std::make_unique<TestAction> (value, 20, 30));
    value = 30;

    // Undo t3
    undo.undo();
    REQUIRE (value == 20);

    // Undo t2
    undo.undo();
    REQUIRE (value == 10);

    // Redo t2
    undo.redo();
    REQUIRE (value == 20);

    // New transaction after partial undo clears redo stack
    undo.beginTransaction ("t4");
    undo.addAction (std::make_unique<TestAction> (value, 20, 99));
    value = 99;

    REQUIRE_FALSE (undo.canRedo());  // t3 is gone

    // Undo t4
    undo.undo();
    REQUIRE (value == 20);

    // Undo t2
    undo.undo();
    REQUIRE (value == 10);

    // Undo t1
    undo.undo();
    REQUIRE (value == 0);

    REQUIRE_FALSE (undo.canUndo());
}
