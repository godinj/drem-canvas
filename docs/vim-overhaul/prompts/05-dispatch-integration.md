# Agent: Dispatch Integration + User Keymap

You are working on the `feature/vim-overhaul` branch of Drem Canvas, a C++17 DAW with Skia rendering and vim-style modal navigation.
Your task is Phase 5: wire `KeymapRegistry` into VimEngine's dispatch path, enable user keymap loading, and eliminate remaining `std::function` callbacks.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions)
- `src/vim/VimEngine.h` (current state after Agents 01/03/04 — thin orchestrator with adapter map)
- `src/vim/VimEngine.cpp` (remaining dispatch logic)
- `src/vim/VimGrammar.h` (ParseResult types — created by Agent 01)
- `src/vim/KeySequence.h` (key sequence parsing — created by Agent 02)
- `src/vim/KeymapRegistry.h` (YAML-backed keymap lookup — created by Agent 02)
- `src/vim/ContextAdapter.h` (adapter interface — created by Agent 03)
- `src/vim/adapters/EditorAdapter.h` (reference adapter — created by Agent 03)
- `src/vim/ActionRegistry.h` (action execution by ID)
- `config/default_keymap.yaml` (default keybindings — created by Agent 02)
- `src/ui/AppController.cpp` (adapter creation, action registration)

## Dependencies

This agent depends on all previous agents (01-04). All adapter files, VimGrammar, KeySequence, KeymapRegistry, and default_keymap.yaml must exist. If any are missing, create stubs from their respective prompt specifications.

## Deliverables

### Migration

#### 1. src/vim/VimEngine.h

Add `KeymapRegistry` as a private member:
```cpp
#include "KeymapRegistry.h"

private:
    KeymapRegistry keymap;
```

Add public methods:
```cpp
void loadDefaultKeymap();
void loadUserKeymap (const std::string& path);
```

Remove ALL remaining `std::function` callback members. After Agents 03/04, what remains should be:
- `onCommandPalette` → becomes an action in ActionRegistry
- `onPluginCommand` → becomes a command-mode action
- `onCreateMidiTrack` → becomes a command-mode action
- `onToggleBrowser` → becomes an action in ActionRegistry
- `onPluginMenuMove/Scroll/Confirm/Cancel/Filter/ClearFilter` → move to a `PluginMenuHandler` internal class or keep inline
- `onLiveMidiNote` → stays (keyboard mode callback, wired by AppController)

For the plugin menu callbacks, since plugin menu is a modal mode (not a panel), keep them as-is or create a small `PluginMenuHandler` struct. This is the one area where callbacks are acceptable since the plugin menu is ephemeral.

Minimal remaining callbacks (if any):
```cpp
// These are mode-based, not panel-based, so they stay on VimEngine
std::function<void (const dc::MidiMessage&)> onLiveMidiNote;
std::function<void (int)> onPluginMenuMove;
std::function<void (int)> onPluginMenuScroll;
std::function<void()> onPluginMenuConfirm;
std::function<void()> onPluginMenuCancel;
std::function<void (const std::string&)> onPluginMenuFilter;
std::function<void()> onPluginMenuClearFilter;
```

Move these to actions in ActionRegistry:
- `onCommandPalette` → action `"command_palette"`, registered by AppController
- `onToggleBrowser` → action `"view.toggle_browser"`, registered by AppController
- `onPluginCommand` → handled by command-mode parsing, action `"command.plugin"`
- `onCreateMidiTrack` → handled by command-mode parsing, action `"command.midi"`

#### 2. src/vim/VimEngine.cpp

**Rewrite `dispatch()` to use keymap-driven dispatch:**

```cpp
bool VimEngine::dispatch (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // 1. Global keymap bindings (Ctrl+P, etc.) — check before mode dispatch
    auto globalAction = keymap.feedKey (VimModes::Normal, context.getPanel(),
                                         keyChar, key.shift, key.control, key.alt, key.command);
    if (globalAction == "command_palette")
    {
        actionRegistry.executeAction ("command_palette");
        keymap.resetFeed();
        return true;
    }

    // 2. Mode-based dispatch (Keyboard, PluginMenu, Command stay internal)
    if (mode == Keyboard)
        return handleKeyboardKey (key);

    if (mode == PluginMenu)
        return handlePluginMenuKey (key);

    if (mode == Command)
        return handleCommandKey (key);

    // 3. Visual modes — delegate to adapter
    if (mode == Visual || mode == VisualLine)
    {
        auto* adapter = getActiveAdapter();
        if (adapter)
        {
            bool handled = (mode == Visual)
                ? adapter->handleVisualKey (key)
                : adapter->handleVisualLineKey (key);
            if (handled) return true;
        }
        return false;
    }

    // 4. Normal mode dispatch
    if (mode == Normal)
        return handleNormalKey (key);

    // 5. Insert mode
    return handleInsertKey (key);
}
```

**Rewrite `handleNormalKey()` to use three-layer dispatch:**

```cpp
bool VimEngine::handleNormalKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();
    auto* adapter = getActiveAdapter();

    // Layer 1: Adapter raw key handling (panels with sub-modes: hint, number entry)
    if (adapter && adapter->wantsRawKeys())
        return adapter->handleRawKey (key);

    // Layer 2: Escape/Ctrl-C always resets
    if (isEscapeOrCtrlC (key))
    {
        grammar.reset();
        keymap.resetFeed();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Layer 3: VimGrammar for compositional commands (operator + count + motion)
    if (adapter)
    {
        grammar.setOperatorChars (adapter->getSupportedOperators());
        grammar.setMotionChars (adapter->getSupportedMotions());
    }

    auto result = grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);

    switch (result.type)
    {
        case VimGrammar::ParseResult::Incomplete:
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case VimGrammar::ParseResult::Motion:
            if (adapter) adapter->executeMotion (result.motionKey, result.count);
            return true;

        case VimGrammar::ParseResult::OperatorMotion:
            if (adapter)
            {
                auto range = adapter->resolveMotion (result.motionKey, result.count);
                if (range.valid)
                    adapter->executeOperator (result.op, range, result.reg);
                listeners.call ([](Listener& l) { l.vimContextChanged(); });
            }
            return true;

        case VimGrammar::ParseResult::LinewiseOperator:
            if (adapter)
            {
                auto range = adapter->resolveLinewiseMotion (result.count);
                if (range.valid)
                    adapter->executeOperator (result.op, range, result.reg);
                listeners.call ([](Listener& l) { l.vimContextChanged(); });
            }
            return true;

        case VimGrammar::ParseResult::NoMatch:
            break;
    }

    // Layer 4: KeymapRegistry for simple action bindings
    int modeInt = static_cast<int> (mode);
    auto actionId = keymap.feedKey (modeInt, context.getPanel(),
                                     keyChar, key.shift, key.control, key.alt, key.command);

    if (actionId == "pending")
        return true; // multi-step binding in progress (e.g., first 'g' of 'gp')

    if (! actionId.empty())
    {
        keymap.resetFeed();
        return actionRegistry.executeAction (actionId);
    }

    keymap.resetFeed();
    return false;
}
```

**`loadDefaultKeymap()`:**
```cpp
void VimEngine::loadDefaultKeymap()
{
    keymap.loadFromYAML (DC_DEFAULT_KEYMAP_PATH);
}
```

**`loadUserKeymap()`:**
```cpp
void VimEngine::loadUserKeymap (const std::string& path)
{
    keymap.overlayFromYAML (path);
}
```

**`handleCommandKey()`:** Convert `onPluginCommand` and `onCreateMidiTrack` to ActionRegistry:
```cpp
void VimEngine::executeCommand()
{
    if (dc::startsWith (commandBuffer, "plugin "))
    {
        // Execute via action registry
        actionRegistry.executeAction ("command.plugin");
    }
    else if (dc::startsWith (commandBuffer, "midi"))
    {
        actionRegistry.executeAction ("command.midi");
    }
}
```

#### 3. src/ui/AppController.cpp

**In constructor / init:**
```cpp
// Load keymaps
vimEngine->loadDefaultKeymap();

// User keymap (optional)
std::string userKeymap = dc::getConfigDir() + "/keymap.yaml";
if (dc::fileExists (userKeymap))
    vimEngine->loadUserKeymap (userKeymap);
```

**Register actions that were callbacks:**
```cpp
vimEngine->getActionRegistry().registerAction ({
    "command_palette", "Command Palette", "Navigation", "Ctrl+P",
    [this]() { showCommandPalette(); }, {}
});

vimEngine->getActionRegistry().registerAction ({
    "view.toggle_browser", "Toggle Browser", "View", "gp",
    [this]() { toggleBrowser(); }, {}
});

vimEngine->getActionRegistry().registerAction ({
    "command.plugin", "Install Plugin", "Command", ":plugin",
    [this]() {
        auto& buf = vimEngine->getCommandBuffer();
        auto name = dc::trim (dc::afterFirst (buf, "plugin "));
        loadPlugin (name);
    }, {}
});

vimEngine->getActionRegistry().registerAction ({
    "command.midi", "Add MIDI Track", "Command", ":midi",
    [this]() {
        auto& buf = vimEngine->getCommandBuffer();
        auto name = dc::trim (dc::afterFirst (buf, "midi"));
        if (name.empty()) name = "MIDI";
        addMidiTrack (name);
    }, {}
});
```

**Adapter action registration:** Each adapter's `registerActions()` is called during adapter creation:
```cpp
auto editorAdapter = std::make_unique<dc::EditorAdapter> (...);
editorAdapter->registerActions (vimEngine->getActionRegistry());
vimEngine->registerAdapter (std::move (editorAdapter));
```

#### 4. User keymap path

Add a helper to find the user config directory. If `dc::getConfigDir()` doesn't exist, create it:

```cpp
// In src/dc/foundation/file_utils.h (or wherever platform paths live)
std::string getConfigDir();  // Returns ~/.config/drem-canvas on Linux, ~/Library/Application Support/DremCanvas on macOS
```

Check if `src/dc/foundation/file_utils.h` already has this. If not, add it.

#### 5. ActionRegistry enhancement

Update `ActionRegistry::executeAction()` to update the `keybinding` display field from KeymapRegistry data when actions are registered. This ensures the command palette shows the user's actual keybindings, not the hardcoded defaults.

Add to ActionRegistry:
```cpp
void updateKeybindings (const KeymapRegistry& keymap, int mode, VimContext::Panel panel);
```

This iterates all actions and calls `keymap.getKeybindingForAction()` to refresh the `keybinding` display string.

### Verification

After completing all changes:

1. **Build:** `cmake --build --preset release`
2. **Tests:** `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
3. **Manual testing:**
   - Launch app, verify all vim keybindings work as before
   - Test all panels: Editor (hjkl, d3j, yy, p), Mixer (strip nav, plugin ops), Sequencer (step toggling), PianoRoll (note editing), PluginView (hint mode)
   - Test visual mode: v, V, selection, operators
   - Test command mode: `:plugin`, `:midi`
   - Test command palette: Ctrl+P, fuzzy search, action execution
   - Create `~/.config/drem-canvas/keymap.yaml` with a test override, verify it takes effect
4. **Run `scripts/verify.sh`**

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run `scripts/verify.sh` before declaring task complete
