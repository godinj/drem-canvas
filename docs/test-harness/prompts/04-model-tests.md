# Agent: Model Tests

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 4 of the test harness: write unit tests for all dc::model types
that replaced JUCE ValueTree and related infrastructure.

## Context

Read these specs before starting:

- `docs/test-harness/02-migration-tests.md` (Phase 1 — Model Tests section)
- `docs/test-harness/00-prd.md` (test directory layout)
- `src/dc/model/Variant.h` + `Variant.cpp` (tagged union replacing juce::var)
- `src/dc/model/PropertyId.h` + `PropertyId.cpp` (string interning)
- `src/dc/model/PropertyTree.h` + `PropertyTree.cpp` (replaces juce::ValueTree)
- `src/dc/model/PropertyTreeActions.h` (undo actions for PropertyTree)
- `src/dc/model/UndoAction.h` (abstract base class)
- `src/dc/model/UndoManager.h` + `UndoManager.cpp` (replaces juce::UndoManager)

Read each source file to understand the exact API before writing tests.

## Prerequisites

Phase 1 (CMake infrastructure) must be completed. The `dc_model` library target
and `dc_unit_tests` executable must exist.

## Deliverables

Create 4 test files in `tests/unit/model/`. Add each file to
`target_sources(dc_unit_tests ...)` in `tests/CMakeLists.txt`.

### 1. tests/unit/model/test_variant.cpp

Test `dc::Variant`:
- All six type tags construct correctly (Void, Int, Double, Bool, String, Binary)
- `int` constructor promotes to `int64_t`
- Strict accessors (`toInt()`, `toString()`, etc.) throw `dc::TypeMismatch` on wrong type
- Fallback accessors (`getIntOr()`, `getDoubleOr()`, etc.) return default on wrong type
- Cross-type conversions:
  - `getIntOr()` on Double (truncates)
  - `getDoubleOr()` on Int (promotes losslessly)
  - `getBoolOr()` on Int (0→false, non-zero→true)
- Equality: same type + value → equal; different types → not equal
- `Void == Void` is true
- Empty string vs Void → not equal
- Binary blob (store and retrieve `vector<uint8_t>`)
- Copy semantics (copy produces equal variant)
- Move semantics (move leaves source as Void)

Use `REQUIRE_THROWS_AS(expr, dc::TypeMismatch)` for exception tests.

### 2. tests/unit/model/test_property_id.cpp

Test `dc::PropertyId`:
- Same string → same pointer (O(1) comparison)
- Different strings → different PropertyIds
- Hash consistency (`Hash{}(id)` is stable across calls)
- Use in `std::unordered_map` (insert and lookup)
- Construct from `const char*` and `string_view` — both produce same PropertyId
- Empty string as PropertyId
- Thread-safe interning (10 threads interning same string simultaneously)

### 3. tests/unit/model/test_property_tree.cpp

Test `dc::PropertyTree` — the most critical test file:

**Property operations:**
- `setProperty()` / `getProperty()` round-trip for all Variant types
- `getProperty()` returns Void for missing key
- `hasProperty()` true after set, false after remove
- `removeProperty()` removes the property
- `getNumProperties()` tracks count
- Setting same value again does not trigger listener

**Child operations:**
- `addChild()` / `getChild()` at various positions (0, middle, end, -1 for append)
- `removeChild(index)` and `removeChild(child)`
- `removeAllChildren()`
- `moveChild(oldIndex, newIndex)`
- `getChild()` out of bounds returns invalid tree
- `getChildWithType()`, `getChildWithProperty()`
- `indexOf()` returns correct index, -1 for non-child

**Parent tracking:**
- `getParent()` returns parent after addChild
- Adding child to new parent removes from old parent
- Root tree getParent returns invalid

**Listener contract:**
- `propertyChanged` fires on setProperty
- `childAdded` fires on addChild
- `childRemoved` fires on removeChild
- `childOrderChanged` fires on moveChild
- `parentChanged` fires when re-parented
- Remove listener during callback — no crash
- Listener on parent notified of child property change

**Deep copy:**
- `createDeepCopy()` produces equal tree
- Mutations on copy do not affect original
- Copy has no parent

**Undo integration:**
- `setProperty(id, val, &undoManager)` records action
- `undo()` restores previous value
- `redo()` re-applies
- `addChild()` with undo, then undo removes child
- `removeChild()` with undo, then undo re-adds child

**Iterator:**
- Range-for over children visits all in order
- Empty tree: `begin() == end()`

Use a test helper to create a mock `PropertyTree::Listener` (with Trompeloeil or
a simple struct that records callback invocations).

### 4. tests/unit/model/test_undo_manager.cpp

Test `dc::UndoManager`:
- Single action: undo restores, redo re-applies
- Transaction grouping: `beginTransaction()` + N actions → single undo step
- Redo stack cleared on new action after undo
- Action coalescing via `tryMerge()`
- `setMaxTransactions()` discards oldest
- `canUndo()` / `canRedo()` state
- `undo()` / `redo()` on empty stack returns false
- `clearHistory()` empties both stacks
- `getUndoDescription()` / `getRedoDescription()` return action names

Create concrete `UndoAction` subclasses for testing (e.g., a simple counter
increment/decrement action).

## Important

- Read the actual source code in `src/dc/model/` before writing tests.
- PropertyTree is the single source of truth for all model state in the application.
  These tests are the highest priority in the entire test harness.
- The PropertyTree listener tests should verify the exact callback signatures and
  arguments. Use Trompeloeil `REQUIRE_CALL` with argument matchers, or use a simple
  recording listener struct.
- PropertyTree undo integration tests need both `PropertyTree` and `UndoManager` — test
  them together.
- Never include `<JuceHeader.h>` in test files.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `tests/CMakeLists.txt` `target_sources(dc_unit_tests ...)`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure`
