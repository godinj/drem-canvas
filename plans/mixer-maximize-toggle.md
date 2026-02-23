# Plan: Toggle Mixer Full-Window with Ctrl+M

## Context

The mixer currently occupies the bottom 35% of the window in a resizable split with the arrangement view. There is no way to expand it to fill the entire content area. This feature adds a `Ctrl+M` keymap in Normal mode that toggles the mixer between its normal split position and a maximized state occupying the full window (minus top bar and status bar).

## Approach

Store a `mixerMaximized` boolean in `VimContext`, toggle it from `VimEngine` on `Ctrl+M`, and have `MainComponent` respond to the `vimContextChanged` listener callback by adjusting the `StretchableLayoutManager` proportions and hiding/showing the arrangement view and resizer bar.

## Changes

### 1. `src/vim/VimContext.h` — Add maximize state

- Add `bool mixerMaximized = false` private member
- Add `bool isMixerMaximized() const` getter
- Add `void toggleMixerMaximized()` method

### 2. `src/vim/VimContext.cpp` — Implement toggle

- Add `toggleMixerMaximized()`: flips the bool

### 3. `src/vim/VimEngine.h` — Declare new action

- Add `void toggleMixerMaximize()` private method declaration

### 4. `src/vim/VimEngine.cpp` — Wire Ctrl+M keybinding

- In `handleNormalKey()`, Phase 7 (non-count actions section), add before the existing Tab handler:
  ```cpp
  if (keyChar == 'm' && modifiers.isCtrlDown()) { toggleMixerMaximize(); return true; }
  ```
- Implement `toggleMixerMaximize()`:
  ```cpp
  void VimEngine::toggleMixerMaximize()
  {
      context.toggleMixerMaximized();
      listeners.call (&Listener::vimContextChanged);
  }
  ```

### 5. `src/gui/MainComponent.h` — Implement VimEngine::Listener

- Add `VimEngine::Listener` as a base class
- Declare `vimModeChanged()` and `vimContextChanged()` overrides

### 6. `src/gui/MainComponent.cpp` — Respond to mixer maximize toggle

- Register `MainComponent` as a `VimEngine::Listener` in the constructor (after creating vimEngine)
- Remove listener in destructor
- Implement `vimContextChanged()`:
  - Read `vimContext.isMixerMaximized()`
  - If maximized: hide `arrangementView` and `layoutResizer`, set layout items so mixer gets 100%
  - If normal: show `arrangementView` and `layoutResizer`, restore default 65/35 split
  - Call `resized()` to re-lay-out

Implementation detail — when maximized, adjust the `StretchableLayoutManager` items:
```cpp
void MainComponent::vimContextChanged()
{
    if (vimContext.isMixerMaximized())
    {
        arrangementView->setVisible (false);
        layoutResizer.setVisible (false);
        layout.setItemLayout (0, 0, 0, 0);        // arrangement: 0
        layout.setItemLayout (1, 0, 0, 0);         // resizer: 0
        layout.setItemLayout (2, 100, -1.0, -1.0); // mixer: 100%
    }
    else
    {
        arrangementView->setVisible (true);
        layoutResizer.setVisible (true);
        layout.setItemLayout (0, 100, -1.0, -0.65);
        layout.setItemLayout (1, 4, 4, 4);
        layout.setItemLayout (2, 100, -1.0, -0.35);
    }
    resized();
}
```

### 7. `src/gui/vim/VimStatusBar.cpp` — Show maximize indicator

- In the context panel segment, append "[MAX]" when `context.isMixerMaximized()` is true, e.g. display "Mixer [MAX]"

## Files Modified

| File | Change |
|------|--------|
| `src/vim/VimContext.h` | Add `mixerMaximized` bool, getter, toggle method |
| `src/vim/VimContext.cpp` | Implement `toggleMixerMaximized()` |
| `src/vim/VimEngine.h` | Declare `toggleMixerMaximize()` |
| `src/vim/VimEngine.cpp` | Add `Ctrl+M` binding + implement action |
| `src/gui/MainComponent.h` | Add `VimEngine::Listener` interface |
| `src/gui/MainComponent.cpp` | Implement listener, toggle layout on context change |
| `src/gui/vim/VimStatusBar.cpp` | Show "[MAX]" indicator in context segment |

## Verification

1. `cmake --build build` — must compile cleanly
2. Open the app: `open "build/DremCanvas_artefacts/Release/Drem Canvas.app"`
3. Press `Ctrl+M` — mixer should expand to fill the full content area (arrangement view and resizer hidden)
4. Status bar should show "Mixer [MAX]"
5. Press `Ctrl+M` again — arrangement view and resizer should reappear at 65/35 split
6. Drag the resizer to a custom position, press `Ctrl+M` twice — should restore to default 65/35 (not custom position, which is acceptable)
7. Press `Tab` to cycle panel focus — should still work in both states
