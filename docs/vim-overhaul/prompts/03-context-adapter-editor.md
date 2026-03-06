# Agent: ContextAdapter Interface + EditorAdapter

You are working on the `feature/vim-overhaul` branch of Drem Canvas, a C++17 DAW with Skia rendering and vim-style modal navigation.
Your task is Phase 3: define the `ContextAdapter` abstract interface and extract `EditorAdapter` — moving all Editor-panel logic out of VimEngine into a standalone adapter.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions)
- `src/vim/VimEngine.h` (full file — constructor signature, all public action methods, all callbacks, MotionRange struct)
- `src/vim/VimEngine.cpp` (lines 116-423: handleNormalKey editor dispatch; 427-815: navigation + clip operations; 1433-1696: clip edge helpers + motion resolution + collectClipsForRange; 1698-2022: operator + motion execution; 2024-2684: visual mode — enter/exit/update/execute)
- `src/vim/VimGrammar.h` (Operator enum, ParseResult — created by Agent 01)
- `src/vim/VimContext.h` (full file — VisualSelection, GridVisualSelection structs)
- `src/vim/ActionRegistry.h` (ActionInfo struct, registerAction API)
- `src/model/Project.h`, `src/model/Arrangement.h`, `src/model/Track.h` (model types the adapter will own)
- `src/model/Clipboard.h` (ClipEntry struct, register-based clipboard)
- `src/model/GridSystem.h` (grid snapping, moveByGridUnits)
- `src/engine/TransportController.h` (transport control, getSampleRate)
- `src/utils/UndoSystem.h` (ScopedTransaction)
- `src/ui/AppController.cpp` (lines 973+: registerAllActions — editor action IDs to preserve)
- `tests/integration/test_vim_commands.cpp` (existing VimTestFixture pattern)

## Dependencies

This agent depends on Agent 01 (VimGrammar). If `VimGrammar.h` doesn't exist yet, create a stub header with the `Operator` enum:

```cpp
namespace dc { class VimGrammar { public: enum Operator { OpNone, OpDelete, OpYank, OpChange }; }; }
```

## Deliverables

### New files (src/vim/)

#### 1. ContextAdapter.h

Abstract interface for panel-specific vim semantics.

```cpp
#pragma once
#include "VimGrammar.h"
#include "VimContext.h"
#include "dc/foundation/keycode.h"
#include <string>

namespace dc
{

class ActionRegistry;

class ContextAdapter
{
public:
    virtual ~ContextAdapter() = default;

    // Identity
    virtual VimContext::Panel getPanel() const = 0;

    // --- Motion resolution ---
    struct MotionRange
    {
        int startIndex     = 0;   // primary axis start (track, strip, row)
        int endIndex       = 0;   // primary axis end
        int startSecondary = 0;   // secondary axis start (clip, step, beat)
        int endSecondary   = 0;   // secondary axis end
        int64_t startPos   = 0;   // time position start (samples)
        int64_t endPos     = 0;   // time position end (samples)
        bool linewise      = false;
        bool valid         = false;
    };

    virtual MotionRange resolveMotion (char32_t motionKey, int count) const = 0;
    virtual MotionRange resolveLinewiseMotion (int count) const = 0;

    // --- Execute standalone motion (cursor movement without operator) ---
    virtual void executeMotion (char32_t motionKey, int count) = 0;

    // --- Execute operator on range ---
    virtual void executeOperator (VimGrammar::Operator op,
                                  const MotionRange& range, char reg) = 0;

    // --- Supported grammar characters for this panel ---
    virtual std::string getSupportedMotions() const = 0;
    virtual std::string getSupportedOperators() const = 0;

    // --- Raw key handling for sub-modes (hint mode, number entry, etc.) ---
    // Return true if this panel wants to intercept keys before grammar/keymap.
    virtual bool wantsRawKeys() const { return false; }
    virtual bool handleRawKey (const dc::KeyPress& key) { return false; }

    // --- Register panel-specific actions with the action registry ---
    virtual void registerActions (ActionRegistry& registry) = 0;

    // --- Visual mode support ---
    virtual void enterVisualMode() {}
    virtual void enterVisualLineMode() {}
    virtual void exitVisualMode() {}
    virtual void updateVisualSelection() {}
    virtual bool handleVisualKey (const dc::KeyPress& key) { return false; }
    virtual bool handleVisualLineKey (const dc::KeyPress& key) { return false; }
};

} // namespace dc
```

#### 2. adapters/EditorAdapter.h

```cpp
#pragma once
#include "vim/ContextAdapter.h"
#include "dc/foundation/listener_list.h"
#include <functional>
#include <vector>

namespace dc
{

class Project;
class Arrangement;
class Track;
class TransportController;
class GridSystem;
class VimEngine;

class EditorAdapter : public ContextAdapter
{
public:
    EditorAdapter (Project& project, Arrangement& arrangement,
                   TransportController& transport, GridSystem& gridSystem,
                   VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::Editor; }

    // Motion resolution (from VimEngine::resolveMotion, lines 1466-1589)
    MotionRange resolveMotion (char32_t motionKey, int count) const override;
    MotionRange resolveLinewiseMotion (int count) const override;

    // Motion execution (from VimEngine::executeMotion, lines 1820-2022)
    void executeMotion (char32_t motionKey, int count) override;

    // Operator execution (from VimEngine::executeOperator, lines 1698-1818)
    void executeOperator (VimGrammar::Operator op,
                          const MotionRange& range, char reg) override;

    std::string getSupportedMotions() const override { return "hjkl0$GgwbeW"; }
    std::string getSupportedOperators() const override { return "dyc"; }

    // Register editor actions with the action registry
    void registerActions (ActionRegistry& registry) override;

    // Visual mode (from VimEngine lines 2024-2684)
    void enterVisualMode() override;
    void enterVisualLineMode() override;
    void exitVisualMode() override;
    void updateVisualSelection() override;
    bool handleVisualKey (const dc::KeyPress& key) override;
    bool handleVisualLineKey (const dc::KeyPress& key) override;

    // --- Public action methods (moved from VimEngine) ---
    // Navigation
    void moveSelectionUp();
    void moveSelectionDown();
    void moveSelectionLeft();
    void moveSelectionRight();
    void jumpToFirstTrack();
    void jumpToLastTrack();

    // Transport
    void jumpToSessionStart();
    void jumpToSessionEnd();
    void togglePlayStop();

    // Clip operations
    void deleteSelectedRegions (char reg = '\0');
    void yankSelectedRegions (char reg = '\0');
    void pasteAfterPlayhead (char reg = '\0');
    void pasteBeforePlayhead (char reg = '\0');
    void splitRegionAtPlayhead();
    void duplicateSelectedClip();

    // Track state
    void toggleMute();
    void toggleSolo();
    void toggleRecordArm();

    // Grid
    void adjustGridDivision (int delta);

    // Listener proxy — adapter needs to notify VimEngine listeners
    using ContextChangedCallback = std::function<void()>;
    using ModeChangedCallback = std::function<void (int newMode)>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }
    void setModeChangedCallback (ModeChangedCallback cb) { onModeChanged = cb; }

    // Callbacks that reach up to AppController (moved from VimEngine)
    std::function<void (const PropertyTree&)> onOpenPianoRoll;

private:
    void updateClipIndexFromGridCursor();

    // Clip collection (from VimEngine::collectClipsForRange, lines 1612-1696)
    struct ClipEntry { PropertyTree data; int trackOffset; int64_t timeOffset; };
    std::vector<ClipEntry> collectClipsForRange (const MotionRange& range) const;

    // Operator helpers
    void executeDelete (const MotionRange& range, char reg);
    void executeYank (const MotionRange& range, char reg);
    void executeChange (const MotionRange& range, char reg);

    // Visual mode helpers (from VimEngine lines 2057-2419)
    MotionRange getVisualRange() const;
    void executeVisualOperator (VimGrammar::Operator op, char reg);
    void executeGridVisualDelete (char reg);
    void executeGridVisualYank (bool isYank, char reg);
    void executeVisualMute();
    void executeVisualSolo();

    // Clip gap carving (from VimEngine.cpp static carveGap, lines 609-661)
    static void carveGap (Track& track, int64_t gapStart, int64_t gapEnd, UndoManager& um);

    Project& project;
    Arrangement& arrangement;
    TransportController& transport;
    GridSystem& gridSystem;
    VimContext& context;

    // Visual mode anchor (moved from VimEngine)
    int visualAnchorTrack = 0;
    int visualAnchorClip  = 0;
    int64_t visualAnchorGridPos = 0;

    ContextChangedCallback onContextChanged;
    ModeChangedCallback onModeChanged;
};

} // namespace dc
```

#### 3. adapters/EditorAdapter.cpp

Move the following methods from `VimEngine.cpp` into `EditorAdapter.cpp`, changing `VimEngine::` prefix to `EditorAdapter::`:

**Navigation (lines 427-501):**
- `moveSelectionUp()`, `moveSelectionDown()`, `moveSelectionLeft()`, `moveSelectionRight()`
- `updateClipIndexFromGridCursor()`

**Track jumps (lines 505-524):**
- `jumpToFirstTrack()`, `jumpToLastTrack()`

**Transport (lines 528-558):**
- `jumpToSessionStart()`, `jumpToSessionEnd()`, `togglePlayStop()`

**Clip operations (lines 562-815):**
- `deleteSelectedRegions()`, `yankSelectedRegions()`
- `pasteAfterPlayhead()`, `pasteBeforePlayhead()`
- `splitRegionAtPlayhead()`, `duplicateSelectedClip()`
- Static `carveGap()` helper (lines 609-661)

**Track state (lines 817-853):**
- `toggleMute()`, `toggleSolo()`, `toggleRecordArm()`

**Clip edge helpers (lines 1433-1455):**
- Static `collectClipEdges()` function

**Motion resolution (lines 1459-1608):**
- `resolveMotion()` — translate VimEngine::MotionRange to ContextAdapter::MotionRange
- `resolveLinewiseMotion()`

**Collect clips for range (lines 1612-1696):**
- `collectClipsForRange()` — use internal ClipEntry instead of Clipboard::ClipEntry

**Operator execution (lines 1698-1818):**
- `executeOperator()` — dispatch to executeDelete/Yank/Change
- `executeDelete()`, `executeYank()`, `executeChange()`

**Motion execution (lines 1820-2022):**
- `executeMotion()` — the full switch statement handling h/j/k/l/0/$G/g/w/b/e

**Visual mode (lines 2024-2684):**
- `enterVisualMode()`, `enterVisualLineMode()`, `exitVisualMode()`
- `updateVisualSelection()`
- `executeVisualOperator()`, `executeGridVisualDelete()`, `executeGridVisualYank()`
- `executeVisualMute()`, `executeVisualSolo()`
- `handleVisualKey()`, `handleVisualLineKey()`

**Key change:** Replace all `listeners.call([](Listener& l) { l.vimContextChanged(); })` with `if (onContextChanged) onContextChanged()`. Replace mode change listeners with `if (onModeChanged) onModeChanged(newMode)`.

**`registerActions()`:** Register editor-specific actions:
```cpp
void EditorAdapter::registerActions (ActionRegistry& registry)
{
    registry.registerAction ({ "nav.move_up", "Move Up", "Navigation", "k",
        [this]() { moveSelectionUp(); }, { VimContext::Editor } });
    registry.registerAction ({ "nav.move_down", "Move Down", "Navigation", "j",
        [this]() { moveSelectionDown(); }, { VimContext::Editor } });
    // ... all editor actions from AppController::registerAllActions()
}
```

### Migration

#### 4. src/vim/VimEngine.h

- Add `#include "ContextAdapter.h"` and `#include <unordered_map>` and `#include <memory>`
- Add adapter storage: `std::unordered_map<int, std::unique_ptr<ContextAdapter>> adapters;`
- Add: `void registerAdapter (std::unique_ptr<ContextAdapter> adapter);`
- Add: `ContextAdapter* getActiveAdapter() const;`
- Remove Editor-specific public action methods: `moveSelectionUp/Down/Left/Right`, `jumpToFirstTrack`, `jumpToLastTrack`, `jumpToSessionStart/End`, `togglePlayStop`, `deleteSelectedRegions`, `yankSelectedRegions`, `pasteAfterPlayhead/BeforePlayhead`, `splitRegionAtPlayhead`, `duplicateSelectedClip`, `toggleMute/Solo/RecordArm`, `enterVisualMode/LineMode`, `exitVisualMode`
- Remove Editor-specific callbacks: `onOpenPianoRoll`
- Remove DAW type references from constructor: `Project&`, `Arrangement&`, `TransportController&`, `GridSystem&` — constructor becomes `VimEngine (VimContext& context)`
- Remove private members: `project`, `transport`, `arrangement`, `gridSystem`, `visualAnchorTrack/Clip/GridPos`
- Remove private methods: `resolveMotion`, `resolveLinewiseMotion`, `executeOperator`, `executeDelete`, `executeYank`, `executeChange`, `executeMotion`, `collectClipsForRange`, `getVisualRange`, `updateVisualSelection`, `executeVisualOperator`, `executeGridVisualDelete`, `executeGridVisualYank`, `executeVisualMute`, `executeVisualSolo`, `handleVisualKey`, `handleVisualLineKey`, `updateClipIndexFromGridCursor`, `openFocusedItem`
- Keep: `handleNormalKey`, `handleInsertKey`, `handleCommandKey`, `handleKeyboardKey`, `handlePluginMenuKey`, `handlePluginSearchKey` (non-editor panel handlers stay for now)
- Keep: all non-editor callbacks (mixer, plugin, piano roll, sequencer, keyboard)
- Keep: `cycleFocusPanel()` (cross-panel, stays in VimEngine)

#### 5. src/vim/VimEngine.cpp

- `handleNormalKey()`: For Editor panel, delegate to adapter:
  ```cpp
  if (context.getPanel() == VimContext::Editor)
  {
      auto* adapter = getActiveAdapter();
      if (adapter)
      {
          // Grammar handles operator/motion composition
          auto result = grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);
          // ... dispatch based on result type, calling adapter methods
      }
  }
  ```
- For Visual/VisualLine modes, delegate to adapter's handleVisualKey/handleVisualLineKey
- Remove all moved method implementations
- Remove static `carveGap` and `collectClipEdges`
- `registerAdapter()`: store adapter in map by panel enum
- `getActiveAdapter()`: return `adapters[context.getPanel()]` or nullptr
- `handleNormalKey()` non-editor panels: keep existing handler calls for now (Mixer, Sequencer, PianoRoll, PluginView will move in Agent 04)

#### 6. src/ui/AppController.h / AppController.cpp

- Create `EditorAdapter` in AppController constructor, passing Project, Arrangement, Transport, GridSystem, VimContext
- Set the adapter's callbacks: `onContextChanged`, `onModeChanged`, `onOpenPianoRoll`
- Call `vimEngine->registerAdapter (std::move (editorAdapter))`
- Remove editor-specific callback wiring that was on VimEngine (the editor callbacks now live on EditorAdapter)
- Keep all non-editor callback wiring on VimEngine (mixer, plugin, piano roll, sequencer, keyboard) — those move in Agent 04
- Update `registerAllActions()`: editor actions now registered by EditorAdapter::registerActions(), remove them from AppController

#### 7. CMakeLists.txt

Add `src/vim/adapters/EditorAdapter.cpp` to target_sources in both:
- Main executable target
- `dc_integration_tests` in tests/CMakeLists.txt

#### 8. tests/integration/test_vim_commands.cpp

Update `VimTestFixture`:
```cpp
struct VimTestFixture
{
    dc::Project project;
    dc::Arrangement arrangement { project };
    dc::TransportController transport;
    dc::VimContext context;
    dc::TempoMap tempoMap;
    dc::GridSystem gridSystem { tempoMap };
    dc::VimEngine engine { context };  // no longer takes DAW types
    dc::EditorAdapter editorAdapter { project, transport, arrangement, gridSystem, context };

    VimTestFixture()
    {
        engine.registerAdapter (std::make_unique<dc::EditorAdapter> (
            project, transport, arrangement, gridSystem, context));
    }
    // ...
};
```

Update test calls: `f.engine.moveSelectionDown()` → get the adapter and call its method, or use the ActionRegistry.

## Scope Limitation

Do NOT move Mixer, Sequencer, PianoRoll, or PluginView handlers — those are Agent 04's scope. Keep VimEngine's `handleMixerNormalKey()`, `handleSequencerNormalKey()`, `handlePianoRollNormalKey()`, `handlePluginViewNormalKey()` intact. Only extract the Editor panel logic and the shared grammar infrastructure.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"vim/adapters/EditorAdapter.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run `scripts/verify.sh` before declaring task complete
