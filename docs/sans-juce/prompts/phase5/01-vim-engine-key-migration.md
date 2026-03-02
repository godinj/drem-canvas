# Agent: VimEngine Key Event Migration

You are working on the `feature/sans-juce-cleanup` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 5 (Final Cleanup): replace all `juce::KeyListener`, `juce::KeyPress`,
and `juce_wchar` usage in the Vim engine with custom `dc::KeyCode` and `char32_t` types.

## Context

Read these specs before starting:
- `docs/sans-juce/08-migration-guide.md` (Phase 5 — VimEngine cleanup)
- `src/vim/VimEngine.h` (class declaration: `juce::KeyListener` base, `keyPressed` override, 12 private `handleXKey(const juce::KeyPress&)` methods, `juce_wchar pendingKey`)
- `src/vim/VimEngine.cpp` (48 `juce::KeyPress` refs, `handleKeyEvent()` bridge at line 78)
- `src/vim/VirtualKeyboardState.h` (`juce_wchar` in `keyToNote()` parameter)
- `src/graphics/core/Event.h` (existing `gfx::KeyEvent` struct — the input event type)

## Deliverables

### New files (src/dc/foundation/)

#### 1. keycode.h

Portable key code enum replacing `juce::KeyPress` named constants.

```cpp
#pragma once
#include <cstdint>

namespace dc
{

enum class KeyCode : int
{
    // Printable characters use their char32_t value directly (cast).
    // These named constants cover non-printable / special keys.
    Unknown      = 0,
    Escape       = 0x1B,
    Return       = 0x0D,
    Tab          = 0x09,
    Space        = 0x20,
    Backspace    = 0x08,
    Delete       = 0x7F,
    UpArrow      = 0xF700,
    DownArrow    = 0xF701,
    LeftArrow    = 0xF702,
    RightArrow   = 0xF703,
    Home         = 0xF729,
    End          = 0xF72B,
    PageUp       = 0xF72C,
    PageDown     = 0xF72D,
    F1           = 0xF704,
    F2           = 0xF705,
    F3           = 0xF706,
    F4           = 0xF707,
    F5           = 0xF708,
    F6           = 0xF709,
    F7           = 0xF70A,
    F8           = 0xF70B,
    F9           = 0xF70C,
    F10          = 0xF70D,
    F11          = 0xF70E,
    F12          = 0xF70F,
};

/// Lightweight key press: key code + text character + modifiers.
/// Replaces juce::KeyPress. Constructed from gfx::KeyEvent in VimEngine.
struct KeyPress
{
    KeyCode code = KeyCode::Unknown;
    char32_t textCharacter = 0;
    bool shift   = false;
    bool control = false;
    bool alt     = false;
    bool command = false;

    /// Convenience: get the text character for comparisons (like key == 'j')
    char32_t getTextCharacter() const { return textCharacter; }

    /// Convenience: check if this is a specific KeyCode
    bool isKeyCode (KeyCode k) const { return code == k; }

    bool operator== (KeyCode k) const { return code == k; }
    bool operator!= (KeyCode k) const { return code != k; }
};

} // namespace dc
```

### Migration

#### 2. src/vim/VimEngine.h

Remove JUCE dependency entirely.

- Remove `#include <JuceHeader.h>`
- Add `#include "dc/foundation/keycode.h"` and `#include "graphics/core/Event.h"`
- Remove `public juce::KeyListener` from the class inheritance
- Remove the `keyPressed (const juce::KeyPress&, juce::Component*)` override (line 47)
- Change `handleKeyEvent(const gfx::KeyEvent&)` to be the primary public dispatch entry point (it already is in practice)
- Change all 12 private `handleXKey` method signatures from `const juce::KeyPress&` to `const dc::KeyPress&`:
  - `handleNormalKey`, `handleInsertKey`, `handleCommandKey`, `handleKeyboardKey`
  - `handlePluginMenuKey`, `handlePluginSearchKey`, `handleVisualKey`, `handleVisualLineKey`
  - `handleSequencerNormalKey`, `handlePianoRollNormalKey`, `handleMixerNormalKey`, `handlePluginViewNormalKey`
- Replace `juce_wchar pendingKey` with `char32_t pendingKey`
- Replace all `juce_wchar` parameter types with `char32_t` (in `isDigitForCount`, `accumulateDigit`, `charToOperator`, `isMotionKey`, `resolveMotion`, `executeMotion`)

#### 3. src/vim/VimEngine.cpp

Rewrite the key dispatch to use `dc::KeyPress` instead of `juce::KeyPress`.

- Remove any `#include <JuceHeader.h>` (if present via the header)
- **Rewrite `handleKeyEvent()`** (line 78): Instead of converting `gfx::KeyEvent` → `juce::KeyPress` → `keyPressed()`, convert `gfx::KeyEvent` → `dc::KeyPress` and dispatch directly:

```cpp
bool VimEngine::handleKeyEvent (const gfx::KeyEvent& event)
{
    dc::KeyCode code = dc::KeyCode::Unknown;

    switch (event.keyCode)
    {
        case 0x35: code = dc::KeyCode::Escape;     break;
        case 0x24: code = dc::KeyCode::Return;      break;
        case 0x30: code = dc::KeyCode::Tab;          break;
        case 0x31: code = dc::KeyCode::Space;        break;
        case 0x33: code = dc::KeyCode::Backspace;    break;
        case 0x7E: code = dc::KeyCode::UpArrow;      break;
        case 0x7D: code = dc::KeyCode::DownArrow;    break;
        case 0x7B: code = dc::KeyCode::LeftArrow;    break;
        case 0x7C: code = dc::KeyCode::RightArrow;   break;
        case 0x75: code = dc::KeyCode::Delete;       break;
        default:
            code = static_cast<dc::KeyCode> (static_cast<int> (event.character));
            break;
    }

    auto textChar = event.unmodifiedCharacter ? event.unmodifiedCharacter : event.character;

    dc::KeyPress key { code, textChar, event.shift, event.control, event.alt, event.command };
    return dispatch (key);
}
```

- **Rename the old `keyPressed()` body to `dispatch(const dc::KeyPress&)`** (private method). This is the master dispatcher that calls `handleNormalKey`, `handleInsertKey`, etc.
- **Delete the `keyPressed(const juce::KeyPress&, juce::Component*)` override entirely**
- **Replace all `juce::KeyPress` comparisons** throughout the file. Common patterns:
  - `key == juce::KeyPress::escapeKey` → `key == dc::KeyCode::Escape`
  - `key == juce::KeyPress::returnKey` → `key == dc::KeyCode::Return`
  - `key == juce::KeyPress::tabKey` → `key == dc::KeyCode::Tab`
  - `key == juce::KeyPress::spaceKey` → `key == dc::KeyCode::Space`
  - `key == juce::KeyPress::backspaceKey` → `key == dc::KeyCode::Backspace`
  - `key == juce::KeyPress::upKey` → `key == dc::KeyCode::UpArrow`
  - `key == juce::KeyPress::downKey` → `key == dc::KeyCode::DownArrow`
  - `key == juce::KeyPress::leftKey` → `key == dc::KeyCode::LeftArrow`
  - `key == juce::KeyPress::rightKey` → `key == dc::KeyCode::RightArrow`
  - `key == juce::KeyPress::deleteKey` → `key == dc::KeyCode::Delete`
- **Replace `key.getTextCharacter()`** — the dc::KeyPress already has this method, so no change needed in call sites
- **Replace `key.getModifiers().isCtrlDown()`** etc with `key.control`, `key.shift`, `key.alt`, `key.command`
- **Replace `juce_wchar` with `char32_t`** everywhere (8 occurrences in .cpp)
- **Update `isEscapeOrCtrlC()` helper** (line ~14):
  ```cpp
  static bool isEscapeOrCtrlC (const dc::KeyPress& key)
  {
      return key == dc::KeyCode::Escape
          || (key.control && key.getTextCharacter() == 'c');
  }
  ```
- **Update `handleKeyUp()`** (line ~3121): Replace `juce_wchar` cast with `char32_t`

#### 4. src/vim/VirtualKeyboardState.h

- Remove `#include <JuceHeader.h>`
- Add `#include <cstdint>` (for char32_t if needed)
- Replace `juce_wchar` parameter in `keyToNote()` with `char32_t`

## Scope Limitation

- Do NOT modify any files outside `src/vim/` and `src/dc/foundation/`
- Do NOT touch `src/gui/` files (they will be deleted by Agent 05)
- Do NOT modify `CMakeLists.txt` (keycode.h is header-only, no .cpp to add)
- The `handleKeyEvent()` platform key code switch uses macOS virtual key codes (0x35, 0x24, etc.) — these come from the platform layer and must remain as-is. On Linux, GLFW key codes are translated before reaching VimEngine (see `GlfwWindow.cpp`)
- Verify that no file in `src/vim/` contains `juce::` or `juce_wchar` after migration

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"dc/foundation/keycode.h"`)
- Build verification: `cmake --build --preset release`
