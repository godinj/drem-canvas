# Agent: Secondary Panel Adapters

You are working on the `feature/vim-overhaul` branch of Drem Canvas, a C++17 DAW with Skia rendering and vim-style modal navigation.
Your task is Phase 4: extract the Mixer, Sequencer, PianoRoll, and PluginView panel handlers from VimEngine into standalone ContextAdapter implementations.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions)
- `src/vim/ContextAdapter.h` (abstract interface — created by Agent 03)
- `src/vim/adapters/EditorAdapter.h` (reference implementation — created by Agent 03)
- `src/vim/VimEngine.h` (remaining callbacks and handler declarations)
- `src/vim/VimEngine.cpp`:
  - Mixer: `handleMixerNormalKey()` (line 3150), `getMixerPluginCount()` (line 3129)
  - Sequencer: `handleSequencerNormalKey()` (line 2727), `seqMove*()` (lines 2817-3016)
  - PianoRoll: `handlePianoRollNormalKey()` (line 1119), `prMove*()`, `closePianoRoll()` (line 1102)
  - PluginView: `handlePluginViewNormalKey()` (line 3509), `openPluginView()` (line 3487), `closePluginView()` (line 3498), `generateHintLabel()`, `resolveHintLabel()` (lines 3437-3485)
- `src/vim/VimContext.h` (MixerFocus enum, HintMode enum, sequencer/plugin state)
- `src/ui/AppController.cpp` (lines 109-578: all callback wiring for mixer, plugin, piano roll)
- `src/model/StepSequencer.h` (step sequencer model)

## Dependencies

This agent depends on Agent 01 (VimGrammar) and Agent 03 (ContextAdapter interface + EditorAdapter). If those files don't exist yet, create stub headers with the interfaces defined in their respective prompts.

## Deliverables

### New files (src/vim/adapters/)

#### 1. MixerAdapter.h / MixerAdapter.cpp

Extract from `VimEngine::handleMixerNormalKey()` (lines 3150-3435) and `getMixerPluginCount()` (line 3129).

```cpp
class MixerAdapter : public ContextAdapter
{
public:
    MixerAdapter (Arrangement& arrangement, VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::Mixer; }

    // Mixer doesn't use the grammar (no operators/motions in vim sense)
    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    // Mixer handles all keys directly (focus cycling, strip selection, plugin ops)
    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    // Callbacks (moved from VimEngine)
    std::function<void (int trackIdx, int pluginIdx)> onMixerPluginOpen;
    std::function<void (int trackIdx)> onMixerPluginAdd;
    std::function<void (int trackIdx, int pluginIdx)> onMixerPluginRemove;
    std::function<void (int trackIdx, int pluginIdx)> onMixerPluginBypass;
    std::function<void (int trackIdx, int fromIdx, int toIdx)> onMixerPluginReorder;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    int getMixerPluginCount() const;

    Arrangement& arrangement;
    VimContext& context;
    ContextChangedCallback onContextChanged;
};
```

**`handleRawKey()`:** Move the entire body of `handleMixerNormalKey()` here. This handles:
- h/l: strip navigation (with master strip H/L)
- j/k: focus cycling (Volume→Pan→Plugins, or plugin slot selection)
- Return: open plugin (if plugin focused) or add plugin (if empty slot)
- x: remove plugin
- b: bypass plugin
- J/K: reorder plugins
- Escape: exit mixer focus or panel

Replace `listeners.call(...)` with `if (onContextChanged) onContextChanged()`.

#### 2. SequencerAdapter.h / SequencerAdapter.cpp

Extract from `VimEngine::handleSequencerNormalKey()` (lines 2727-2815) and `seqMove*` / `seqToggle*` / `seqAdjust*` / `seqCycle*` / `seqJump*` methods (lines 2817-3016).

```cpp
class SequencerAdapter : public ContextAdapter
{
public:
    SequencerAdapter (Project& project, VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::Sequencer; }

    // Sequencer doesn't use grammar operators
    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    // Sequencer handles all keys directly
    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    // Navigation
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void jumpFirstStep();
    void jumpLastStep();
    void jumpFirstRow();
    void jumpLastRow();

    // Editing
    void toggleStep();
    void adjustVelocity (int delta);
    void cycleVelocity();
    void toggleRowMute();
    void toggleRowSolo();

    Project& project;
    VimContext& context;
    ContextChangedCallback onContextChanged;
};
```

**`handleRawKey()`:** Move body of `handleSequencerNormalKey()`. Handles:
- h/j/k/l: grid navigation
- 0/$: jump to first/last step
- gg/G: jump to first/last row
- Space: toggle step
- +/-: velocity adjust
- v: cycle velocity
- M/S: row mute/solo

Move all `seqMove*`, `seqJump*`, `seqToggle*`, `seqAdjust*`, `seqCycle*` method bodies.

#### 3. PianoRollAdapter.h / PianoRollAdapter.cpp

Extract from `VimEngine::handlePianoRollNormalKey()` (lines 1119-1365) and `prMove*`, `closePianoRoll()` (line 1102).

```cpp
class PianoRollAdapter : public ContextAdapter
{
public:
    PianoRollAdapter (VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::PianoRoll; }

    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    // Callbacks (moved from VimEngine)
    std::function<void (int tool)> onSetPianoRollTool;
    std::function<void (char reg)> onDeleteSelected;
    std::function<void (char reg)> onCopy;
    std::function<void (char reg)> onPaste;
    std::function<void()> onDuplicate;
    std::function<void (int)> onTranspose;
    std::function<void()> onSelectAll;
    std::function<void()> onQuantize;
    std::function<void()> onHumanize;
    std::function<void (bool)> onVelocityLane;
    std::function<void (float)> onZoom;
    std::function<void()> onZoomToFit;
    std::function<void (int)> onGridDiv;
    std::function<void (int, int)> onMoveCursor;
    std::function<void()> onAddNote;
    std::function<void (int, int)> onJumpCursor;
    std::function<void()> onClose;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    VimContext& context;
    ContextChangedCallback onContextChanged;
};
```

**`handleRawKey()`:** Move body of `handlePianoRollNormalKey()`. Handles:
- h/j/k/l: cursor movement via `onMoveCursor`
- 0/$: jump to start/end
- gg/G: jump to first/last
- 1/2/3 or s/d: tool selection
- x: delete selected
- y: copy
- p: paste
- D: duplicate
- +/-: transpose
- a: select all
- q/Q: quantize/humanize
- v: velocity lane toggle
- zi/zo/zf: zoom
- Return: add note
- [/]: grid division
- Escape: close piano roll

Register prefix handling for "x: the adapter needs its own register accumulation, or accept the register from VimGrammar. Since PianoRoll uses `wantsRawKeys()`, it bypasses the grammar. Add a simple `awaitingRegisterChar` + `pendingRegister` mini-state, or access the grammar's register state.

#### 4. PluginViewAdapter.h / PluginViewAdapter.cpp

Extract from `VimEngine::handlePluginViewNormalKey()` (lines 3509-3778), `openPluginView()` (line 3487), `closePluginView()` (line 3498), `generateHintLabel()`, `resolveHintLabel()` (lines 3437-3485).

```cpp
class PluginViewAdapter : public ContextAdapter
{
public:
    PluginViewAdapter (VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::PluginView; }

    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    // Hint label generation (static, keep accessible)
    static std::string generateHintLabel (int index, int totalCount);
    static int resolveHintLabel (const std::string& label, int totalCount);

    // Callbacks (moved from VimEngine)
    std::function<void()> onRescan;
    std::function<void()> onToggleDragAxis;
    std::function<void()> onToggleDragCenter;
    std::function<void()> onEndDrag;
    std::function<void (int paramIdx, float delta)> onParamAdjust;
    std::function<void (int paramIdx, float newValue)> onParamChanged;
    std::function<int()> onQuerySpatialHintCount;
    std::function<int (int spatialIdx)> onResolveSpatialHint;
    std::function<int()> onQueryParamCount;
    std::function<void()> onClose;
    std::function<void (int trackIdx, int pluginIdx)> onOpen;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    VimContext& context;
    ContextChangedCallback onContextChanged;
};
```

**`handleRawKey()`:** Move body of `handlePluginViewNormalKey()`. Handles:
- Hint mode (HintActive/HintSpatial): accumulate label chars, resolve to parameter
- Number entry mode: accumulate digits, confirm with Return
- h/l: parameter prev/next
- j/k: parameter adjust (large delta)
- J/K: parameter adjust (fine delta)
- f: enter hint mode
- e: open native editor
- z: toggle enlarged
- R: rescan
- x: toggle drag axis
- q: end drag
- c: toggle center on reverse
- Escape: close plugin view

Move `generateHintLabel()` and `resolveHintLabel()` as static methods.

### Migration

#### 5. src/vim/VimEngine.h

Remove all panel-specific handler declarations:
- `handleMixerNormalKey()`, `handleSequencerNormalKey()`, `handlePianoRollNormalKey()`, `handlePluginViewNormalKey()`
- `openPluginView()`, `closePluginView()`, `closePianoRoll()`, `getMixerPluginCount()`, `openFocusedItem()`

Remove all panel-specific public methods:
- `prMoveLeft/Right/Up/Down`, `prSelectNote`, `prDeleteNote`, `prAddNote`, `prJumpStart/End`
- `seqMoveLeft/Right/Up/Down`, `seqJumpFirst/Last Step/Row`, `seqToggleStep`, `seqAdjustVelocity`, `seqCycleVelocity`, `seqToggleRowMute/Solo`

Remove panel-specific callbacks:
- All `onMixerPlugin*` callbacks
- All `onPianoRoll*` callbacks
- All `onPluginView*` / `onPluginParam*` / `onQuery*` callbacks
- `onPluginMenuMove/Scroll/Confirm/Cancel/Filter/ClearFilter` (move to VimEngine's plugin menu handler or a PluginMenuHandler class)
- `onLiveMidiNote` stays on VimEngine (keyboard mode is not panel-specific)

Remove `generateHintLabel()` and `resolveHintLabel()` (moved to PluginViewAdapter).

#### 6. src/vim/VimEngine.cpp

- `handleNormalKey()`: Remove panel-specific dispatch at lines 137-147. Replace with:
  ```cpp
  auto* adapter = getActiveAdapter();
  if (adapter && adapter->wantsRawKeys())
      return adapter->handleRawKey (key);
  ```
- Remove all `handle*NormalKey()` implementations
- Remove all `seqMove*`, `prMove*`, `openPluginView`, `closePluginView`, `closePianoRoll`, `getMixerPluginCount`, `openFocusedItem`
- Remove `generateHintLabel`, `resolveHintLabel`
- Keep: `handleKeyboardKey()`, `handlePluginMenuKey()`, `handlePluginSearchKey()`, `handleCommandKey()`, `handleInsertKey()` — these are mode-based, not panel-based
- Keep: `enterKeyboardMode()`, `exitKeyboardMode()`, `cycleFocusPanel()`, `enterPluginMenuMode()`

#### 7. src/ui/AppController.cpp

- Create all four adapters in constructor, set their callbacks, register with VimEngine:
  ```cpp
  auto mixerAdapter = std::make_unique<dc::MixerAdapter> (arrangement, context);
  mixerAdapter->onMixerPluginOpen = [this] (int t, int p) { ... };
  // ... wire all mixer callbacks
  vimEngine->registerAdapter (std::move (mixerAdapter));
  ```
- Remove the callback wiring that was on VimEngine for mixer, piano roll, plugin view
- Keep `onPluginMenuMove/Confirm/Cancel/etc.` wiring on VimEngine (plugin menu mode is modal, not panel)
- Keep `onLiveMidiNote` on VimEngine

#### 8. CMakeLists.txt

Add to main target_sources:
```cmake
src/vim/adapters/MixerAdapter.cpp
src/vim/adapters/SequencerAdapter.cpp
src/vim/adapters/PianoRollAdapter.cpp
src/vim/adapters/PluginViewAdapter.cpp
```

Add to `dc_integration_tests` in tests/CMakeLists.txt.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"vim/adapters/MixerAdapter.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run `scripts/verify.sh` before declaring task complete
