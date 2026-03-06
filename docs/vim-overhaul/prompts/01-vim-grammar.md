# Agent: Extract VimGrammar State Machine

You are working on the `feature/vim-overhaul` branch of Drem Canvas, a C++17 DAW with Skia rendering and vim-style modal navigation.
Your task is Phase 1: extract the count/operator/motion/register parsing logic from `VimEngine` into a standalone `VimGrammar` class with no DAW dependencies.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, test commands)
- `src/vim/VimEngine.h` (lines 22-24: Mode/Operator enums; lines 245-302: count/operator/register state)
- `src/vim/VimEngine.cpp` (lines 1367-1431: count/operator helpers; lines 1459-1464: isMotionKey; lines 208-230: register prefix handling; lines 232-260: digit accumulation + operator keys)
- `src/dc/foundation/keycode.h` (KeyPress/KeyCode types)
- `tests/integration/test_vim_commands.cpp` (existing test fixture pattern using Catch2)

## Deliverables

### New files (src/vim/)

#### 1. VimGrammar.h

Pure state machine for vim compositional grammar. No DAW includes — only `<string>`, `<cstdint>`.

```cpp
namespace dc
{

class VimGrammar
{
public:
    enum Operator { OpNone, OpDelete, OpYank, OpChange };

    struct ParseResult
    {
        enum Type
        {
            NoMatch,          // key is not part of grammar (fall through to keymap)
            Motion,           // standalone motion (e.g. 3j)
            OperatorMotion,   // operator + motion (e.g. d3j)
            LinewiseOperator, // doubled operator (e.g. dd, 3yy)
            Incomplete        // waiting for more input (operator pending, count in progress)
        };

        Type type = NoMatch;
        char32_t motionKey = 0;
        Operator op = OpNone;
        int count = 1;        // effective count (preCount * postCount)
        char reg = '\0';      // register from "x prefix
    };

    // Feed one key into the grammar. Returns parse result.
    ParseResult feed (char32_t keyChar, bool shift, bool control, bool alt, bool command);

    // Cancel all pending state (Escape/Ctrl-C)
    void reset();

    // State queries (for status bar display)
    bool isOperatorPending() const;
    bool hasPendingState() const;
    std::string getPendingDisplay() const;

    // Configure which characters are operators/motions (panel-dependent)
    void setOperatorChars (const std::string& chars);
    void setMotionChars (const std::string& chars);

    // Pending multi-key support (for "gg" sequences)
    void setPendingKeys (const std::string& chars);  // e.g. "g" for gg
    bool hasPendingKey() const;
    char32_t getPendingKey() const;
    void clearPendingKey();

private:
    Operator pendingOperator = OpNone;
    int preCount = 0;       // count before operator (the 3 in 3d2j)
    int postCount = 0;      // count after operator (the 2 in 3d2j)
    char pendingRegister = '\0';
    bool awaitingRegisterChar = false;
    char32_t pendingKey = 0;
    int64_t pendingTimestamp = 0;
    static constexpr int64_t pendingTimeoutMs = 1000;

    std::string operatorChars = "dyc";
    std::string motionChars = "hjkl0$GgwbeW";
    std::string pendingKeyChars = "g";

    bool isDigitForCount (char32_t c) const;
    bool isOperatorChar (char32_t c) const;
    bool isMotionChar (char32_t c) const;
    bool isPendingKeyChar (char32_t c) const;
    Operator charToOperator (char32_t c) const;
    int getEffectiveCount() const;
};

} // namespace dc
```

#### 2. VimGrammar.cpp

Implement the `feed()` method following the exact phased dispatch from `VimEngine::handleNormalKey()` lines 116-283:

1. **Pending key resolution** — if `pendingKey == 'g'` and next key is `'g'`, return `Motion` with motionKey `'g'` (gg). If timeout or different key, clear pending and fall through.
2. **Register prefix** — if `awaitingRegisterChar`, store register and return `Incomplete`. If key is `"`, set `awaitingRegisterChar = true` and return `Incomplete`.
3. **Digit accumulation** — digits 1-9 always accumulate (0 only if count already started). Return `Incomplete`.
4. **Operator keys** — if already pending same operator, return `LinewiseOperator` (dd/yy/cc). Otherwise set pending and return `Incomplete`.
5. **Motion keys** — if operator pending, return `OperatorMotion`. Otherwise return `Motion`.
6. **No match** — return `NoMatch` so caller can check keymap.

Extract these helpers from VimEngine.cpp:
- `isDigitForCount()` (line 1378) — digits 1-9 always, 0 only when count > 0
- `accumulateDigit()` (line 1390) — `count = count * 10 + (c - '0')`
- `getEffectiveCount()` (line 1400) — `max(1, preCount) * max(1, postCount)`
- `charToOperator()` (line 1425) — d→OpDelete, y→OpYank, c→OpChange
- `isMotionKey()` (line 1459) — check against motionChars string

Use `dc::currentTimeMillis()` from `dc/foundation/time.h` for pending key timeout.

`reset()` clears all state: pendingOperator, both counts, pendingRegister, awaitingRegisterChar, pendingKey.

`getPendingDisplay()` returns a string like `"3d"`, `"d2"`, `""a3d"` for the status bar.

### New files (tests/)

#### 3. tests/unit/vim/test_vim_grammar.cpp

Unit tests using Catch2. Test cases:

- Simple motions: feed('j') → Motion{motionKey='j', count=1}
- Counted motions: feed('3'), feed('j') → first Incomplete, then Motion{motionKey='j', count=3}
- Operator + motion: feed('d'), feed('j') → Incomplete, then OperatorMotion{op=OpDelete, motionKey='j', count=1}
- Counted operator + motion: feed('3'), feed('d'), feed('2'), feed('j') → ..., OperatorMotion{count=6}
- Doubled operator: feed('d'), feed('d') → Incomplete, then LinewiseOperator{op=OpDelete, count=1}
- Counted doubled: feed('3'), feed('d'), feed('d') → LinewiseOperator{count=3}
- Register prefix: feed('"'), feed('a'), feed('d'), feed('j') → ..., OperatorMotion{reg='a'}
- Pending gg: feed('g') → Incomplete, feed('g') → Motion{motionKey='g'}
- Reset cancels: feed('d'), reset() → all state cleared
- Non-grammar key: feed('M') → NoMatch
- Configurable operators: setOperatorChars("d"), then feed('y') → NoMatch

### Migration

#### 4. src/vim/VimEngine.h

Add `#include "VimGrammar.h"`. Add `VimGrammar grammar;` private member. Keep all existing members — this phase does NOT change VimEngine's public API or behavior.

#### 5. src/vim/VimEngine.cpp

In `handleNormalKey()`, replace the inline count/operator/register/motion pipeline (lines 208-283) with calls to `grammar.feed()`:

```cpp
// Phase: Grammar (replaces inline count/operator/motion handling)
auto result = grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);

switch (result.type)
{
    case VimGrammar::ParseResult::Incomplete:
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;

    case VimGrammar::ParseResult::Motion:
        executeMotion (result.motionKey, result.count);
        return true;

    case VimGrammar::ParseResult::OperatorMotion:
    {
        auto range = resolveMotion (result.motionKey, result.count);
        if (range.valid)
            executeOperator (result.op, range);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    case VimGrammar::ParseResult::LinewiseOperator:
    {
        auto range = resolveLinewiseMotion (result.count);
        executeOperator (result.op, range);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    case VimGrammar::ParseResult::NoMatch:
        break; // fall through to phase 6/7 single-key actions
}
```

Remove the now-redundant private members from VimEngine:
- `pendingOperator`, `countAccumulator`, `operatorCount` (moved to VimGrammar)
- `pendingRegister`, `awaitingRegisterChar` (moved to VimGrammar)
- `pendingKey`, `pendingTimestamp`, `pendingTimeoutMs` (moved to VimGrammar)
- Helper methods: `isDigitForCount`, `accumulateDigit`, `getEffectiveCount`, `resetCounts`, `startOperator`, `cancelOperator`, `charToOperator`, `isMotionKey`

Keep `resolveMotion`, `resolveLinewiseMotion`, `executeOperator`, `executeMotion`, `executeDelete`, `executeYank`, `executeChange` — those stay in VimEngine for now.

Update `hasPendingState()` and `getPendingDisplay()` to delegate to `grammar`.

Update `handleVisualKey()` and `handleVisualLineKey()` similarly — they also use count accumulation and operator dispatch. Wire them through `grammar.feed()` the same way.

Update Escape/Ctrl-C handling to call `grammar.reset()`.

Update `consumeRegister()` to read from `grammar` state.

#### 6. CMakeLists.txt

Add `src/vim/VimGrammar.cpp` to `target_sources` for the main executable (near line 253).

#### 7. tests/CMakeLists.txt

Add `unit/vim/test_vim_grammar.cpp` to `dc_unit_tests` sources. No additional app-layer sources needed — VimGrammar has no DAW dependencies.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"vim/VimGrammar.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run `scripts/verify.sh` before declaring task complete
