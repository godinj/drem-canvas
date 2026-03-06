# Agent: Keymap Infrastructure

You are working on the `feature/vim-overhaul` branch of Drem Canvas, a C++17 DAW with Skia rendering and vim-style modal navigation.
Your task is Phase 2: build the `KeySequence` parser, `KeymapRegistry` (YAML-backed key→action lookup), and the default keymap YAML. This is purely additive — nothing uses these types yet.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions)
- `src/dc/foundation/keycode.h` (KeyCode enum, KeyPress struct — the existing key representation)
- `src/vim/VimEngine.h` (lines 22: Mode enum — Normal, Insert, Command, Keyboard, PluginMenu, Visual, VisualLine)
- `src/vim/VimContext.h` (line 11: Panel enum — Editor, Mixer, Sequencer, PianoRoll, PluginView)
- `src/vim/ActionRegistry.h` (ActionInfo struct with `id`, `keybinding` fields)
- `src/vim/VimEngine.cpp` — read through `handleNormalKey()` (116-423), `handleMixerNormalKey()` (3150+), `handleSequencerNormalKey()` (2727+), `handlePianoRollNormalKey()` (1119+), `handlePluginViewNormalKey()` (3509+) to extract all current hardcoded keybindings
- `src/ui/AppController.cpp` (lines 973+: `registerAllActions()` — all action IDs and their keybinding strings)

## Deliverables

### New files (src/vim/)

#### 1. KeySequence.h

Parsed representation of a key sequence like `"Ctrl+Shift+j"`, `"gg"`, `"Space"`.

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace dc
{

struct KeySequence
{
    struct Step
    {
        char32_t character = 0;    // 'j', 'g', etc. or 0 for special keys
        std::string specialKey;    // "Space", "Return", "Tab", "Escape", "Backspace", "Delete"
        bool shift   = false;
        bool control = false;
        bool alt     = false;
        bool command = false;

        bool matches (char32_t keyChar, bool s, bool c, bool a, bool cmd) const;
    };

    std::vector<Step> steps;

    // Parse from string representation: "Ctrl+j", "gg", "Space", "zi"
    static KeySequence parse (const std::string& repr);

    // Convert back to display string
    std::string toString() const;

    // Single-step convenience
    bool isSingleKey() const { return steps.size() == 1; }

    bool operator== (const KeySequence& other) const;
    bool operator< (const KeySequence& other) const;
};

} // namespace dc
```

#### 2. KeySequence.cpp

Implement `parse()`:
- Split on `+` but only for modifier prefixes: `Ctrl+`, `Shift+`, `Alt+`, `Cmd+`
- The final segment is the key character or special name
- Multi-character sequences without `+` are multi-step: `"gg"` → two steps `[{g}, {g}]`, `"zi"` → `[{z}, {i}]`
- Special key names: `Space`, `Return`, `Tab`, `Escape`, `Backspace`, `Delete`, `UpArrow`, `DownArrow`, `LeftArrow`, `RightArrow`
- Case sensitive: `"M"` = shift+m (character 'M'), `"m"` = plain m

Implement `Step::matches()`:
- If `specialKey` is set, compare against known KeyCode values
- Otherwise compare `character` and modifier flags
- Modifiers must match exactly

Implement `toString()`:
- Reconstruct `"Ctrl+Shift+j"` from structured data
- Multi-step: `"gg"`, `"zi"`

#### 3. KeymapRegistry.h

YAML-backed key→action lookup, scoped by mode and panel.

```cpp
#pragma once
#include "KeySequence.h"
#include "VimContext.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace dc
{

// Forward declaration — VimEngine::Mode lives in VimEngine.h
// Use an int alias to avoid circular dependency
using VimMode = int;

// Mode constants matching VimEngine::Mode
namespace VimModes
{
    constexpr int Normal    = 0;
    constexpr int Insert    = 1;
    constexpr int Command   = 2;
    constexpr int Keyboard  = 3;
    constexpr int PluginMenu = 4;
    constexpr int Visual    = 5;
    constexpr int VisualLine = 6;
}

class KeymapRegistry
{
public:
    struct Binding
    {
        KeySequence keys;
        std::string actionId;
        VimMode mode;
        VimContext::Panel panel;
        bool isGlobal;           // true = applies to all panels in this mode
    };

    // Load default keymap (embedded or from file)
    void loadFromYAML (const std::string& path);

    // Overlay user keymap on top of defaults (user bindings override)
    void overlayFromYAML (const std::string& path);

    // Resolve: given mode + panel + key sequence, return action ID (empty if no match)
    std::string resolve (VimMode mode, VimContext::Panel panel,
                         const KeySequence& seq) const;

    // Multi-step resolution: feed one key at a time, returns:
    //   - action ID if fully matched
    //   - "pending" if partial match (more keys needed)
    //   - empty string if no match
    std::string feedKey (VimMode mode, VimContext::Panel panel,
                         char32_t keyChar, bool shift, bool ctrl, bool alt, bool cmd);
    void resetFeed();

    // Reverse lookup: get keybinding display string for an action
    std::string getKeybindingForAction (const std::string& actionId,
                                        VimMode mode, VimContext::Panel panel) const;

    // Get all bindings (for debug/UI display)
    const std::vector<Binding>& getAllBindings() const { return bindings; }

    // Clear all bindings
    void clear();

private:
    std::vector<Binding> bindings;
    std::vector<KeySequence::Step> feedBuffer;

    void parseYAMLSection (const std::string& yamlPath,
                           const std::string& sectionName,
                           VimMode mode, bool isMotionOrOperator);
};

} // namespace dc
```

#### 4. KeymapRegistry.cpp

Implement YAML loading using yaml-cpp (already a project dependency):

**YAML structure:**
```yaml
normal:
  global:
    "M": track.toggle_mute
    "Space": transport.play_stop
  editor:
    "s": edit.split
  mixer:
    "Return": mixer.open_plugin

visual:
  global:
    "d": visual.delete

visual_line:
  global:
    "d": visual.delete
```

**`loadFromYAML()`:**
- Parse YAML file
- For each mode section (`normal`, `visual`, `visual_line`):
  - Parse `global` subsection → bindings with `isGlobal = true`
  - Parse panel subsections (`editor`, `mixer`, `sequencer`, `pianoroll`, `pluginview`) → bindings with specific panel
- For each key-value pair: `KeySequence::parse(key)` → actionId
- Handle `null` values as unbind markers (skip/remove binding)

**`overlayFromYAML()`:**
- Same parsing as loadFromYAML
- For each new binding, remove any existing binding with the same mode+panel+keys
- Then add the new binding
- If value is `null`, just remove (unbind)

**`resolve()`:**
- Search bindings for exact match on mode + panel + keys
- Panel-specific bindings take priority over global bindings
- Return first match's actionId

**`feedKey()`:**
- Append key to `feedBuffer`
- Check all bindings: if any binding's key sequence starts with feedBuffer, it's a potential match
- If exactly one binding matches completely, return its actionId and clear buffer
- If some bindings are partial matches, return `"pending"`
- If no bindings match at all, clear buffer and return `""`

**Panel name mapping:**
- `"editor"` → `VimContext::Editor`
- `"mixer"` → `VimContext::Mixer`
- `"sequencer"` → `VimContext::Sequencer`
- `"pianoroll"` → `VimContext::PianoRoll`
- `"pluginview"` → `VimContext::PluginView`

### New files (config/)

#### 5. config/default_keymap.yaml

Generate by reading ALL current hardcoded keybindings from VimEngine.cpp and AppController.cpp.
This YAML must reproduce the exact current behavior when loaded.

```yaml
# Drem Canvas Default Keymap
# Override in ~/.config/drem-canvas/keymap.yaml

normal:
  global:
    "Ctrl+P": command_palette
    "Tab": nav.cycle_panel
    "Space": transport.play_stop
    "i": mode.insert
    ":": mode.command
    "Ctrl+K": mode.keyboard
    "gp": view.toggle_browser
    "gk": mode.keyboard
    "u": edit.undo
    "Ctrl+z": edit.undo
    "Ctrl+R": edit.redo
    "M": track.toggle_mute
    "S": track.toggle_solo
    "r": track.toggle_record_arm
    "[": grid.coarsen
    "]": grid.refine
    "Return": nav.open_focused

  editor:
    "v": mode.visual
    "V": mode.visual_line
    "x": edit.delete
    "p": edit.paste_after
    "P": edit.paste_before
    "s": edit.split
    "D": edit.duplicate

  mixer:
    "h": mixer.prev_strip
    "l": mixer.next_strip
    "j": mixer.focus_down
    "k": mixer.focus_up
    "Return": mixer.open_or_add_plugin
    "x": mixer.remove_plugin
    "b": mixer.bypass_plugin
    "J": mixer.reorder_down
    "K": mixer.reorder_up
    "H": mixer.select_master
    "L": mixer.deselect_master

  sequencer:
    "h": seq.move_left
    "l": seq.move_right
    "j": seq.move_down
    "k": seq.move_up
    "0": seq.jump_first_step
    "$": seq.jump_last_step
    "gg": seq.jump_first_row
    "G": seq.jump_last_row
    "Space": seq.toggle_step
    "+": seq.velocity_up
    "-": seq.velocity_down
    "v": seq.cycle_velocity
    "M": seq.toggle_row_mute
    "S": seq.toggle_row_solo

  pianoroll:
    "h": pr.move_left
    "l": pr.move_right
    "j": pr.move_down
    "k": pr.move_up
    "1": pr.tool_select
    "s": pr.tool_select
    "2": pr.tool_draw
    "d": pr.tool_draw
    "3": pr.tool_erase
    "x": pr.delete
    "y": pr.copy
    "p": pr.paste
    "D": pr.duplicate
    "+": pr.transpose_up
    "-": pr.transpose_down
    "q": pr.quantize
    "Q": pr.humanize
    "v": pr.velocity_lane
    "zi": pr.zoom_in
    "zo": pr.zoom_out
    "zf": pr.zoom_fit
    "0": pr.jump_start
    "$": pr.jump_end
    "gg": pr.jump_start
    "G": pr.jump_end
    "a": pr.select_all
    "Return": pr.add_note
    "[": pr.grid_coarsen
    "]": pr.grid_refine
    "Escape": pr.close

  pluginview:
    "h": pv.param_prev
    "l": pv.param_next
    "j": pv.param_adjust_down
    "k": pv.param_adjust_up
    "J": pv.param_adjust_down_fine
    "K": pv.param_adjust_up_fine
    "f": pv.hint_mode
    "e": pv.open_native
    "z": pv.toggle_enlarged
    "R": pv.rescan
    "x": pv.toggle_drag_axis
    "q": pv.end_drag
    "c": pv.toggle_center_reverse
    "Escape": pv.close

# Motions participate in the vim grammar (count + operator + motion)
motions:
  editor:
    "j": motion.down
    "k": motion.up
    "h": motion.left
    "l": motion.right
    "0": motion.line_start
    "$": motion.line_end
    "G": motion.file_end
    "gg": motion.file_start
    "w": motion.word_next
    "b": motion.word_prev
    "e": motion.word_end

# Operators combine with motions (e.g., d3j)
operators:
  editor:
    "d": operator.delete
    "y": operator.yank
    "c": operator.change

visual:
  global:
    "Escape": mode.normal
    "v": mode.normal
    "V": mode.visual_line
    "d": visual.delete
    "x": visual.delete
    "y": visual.yank
    "c": visual.change
    "p": visual.paste
    "M": visual.toggle_mute
    "S": visual.toggle_solo
    "j": visual.extend_down
    "k": visual.extend_up
    "h": visual.extend_left
    "l": visual.extend_right
    "G": visual.extend_last
    "gg": visual.extend_first
    "0": visual.extend_line_start
    "$": visual.extend_line_end

visual_line:
  global:
    "Escape": mode.normal
    "V": mode.normal
    "v": mode.visual
    "d": visual.delete
    "y": visual.yank
    "c": visual.change
    "M": visual.toggle_mute
    "S": visual.toggle_solo
    "j": visual.extend_down
    "k": visual.extend_up
    "G": visual.extend_last
    "gg": visual.extend_first
```

### New files (tests/)

#### 6. tests/unit/vim/test_key_sequence.cpp

Catch2 unit tests:
- Parse single key: `KeySequence::parse("j")` → 1 step, character='j'
- Parse modifier: `KeySequence::parse("Ctrl+R")` → 1 step, character='R', control=true
- Parse multi-modifier: `KeySequence::parse("Ctrl+Shift+j")` → shift=true, control=true
- Parse special key: `KeySequence::parse("Space")` → specialKey="Space"
- Parse multi-step: `KeySequence::parse("gg")` → 2 steps [{g}, {g}]
- Parse multi-step different: `KeySequence::parse("zi")` → 2 steps [{z}, {i}]
- Round-trip: `KeySequence::parse(s).toString() == s` for various inputs
- Step::matches: verify modifier matching is exact

#### 7. tests/unit/vim/test_keymap_registry.cpp

Catch2 unit tests:
- Load YAML and resolve simple binding
- Panel-specific override of global binding (e.g., Space in sequencer)
- `null` unbinding removes a binding
- Overlay: user keymap overrides default
- `feedKey()` multi-step: feed 'g', get "pending", feed 'g', get action
- `feedKey()` no match: feed unknown key, get ""
- `getKeybindingForAction()` reverse lookup
- Panel priority: panel-specific wins over global for same key

### Build integration

#### 8. CMakeLists.txt

Add to main target `target_sources` (near line 253):
```cmake
src/vim/KeySequence.cpp
src/vim/KeymapRegistry.cpp
```

#### 9. tests/CMakeLists.txt

Add to `dc_unit_tests`:
```
unit/vim/test_key_sequence.cpp
unit/vim/test_keymap_registry.cpp
```

No additional app-layer sources needed — these types only depend on yaml-cpp (already linked).

#### 10. config/default_keymap.yaml

Install or embed this file. For now, place at `config/default_keymap.yaml` in the source tree. Add a CMake `configure_file` or define a compile-time path so `KeymapRegistry` can find it at runtime:

```cmake
set(DEFAULT_KEYMAP_PATH "${CMAKE_SOURCE_DIR}/config/default_keymap.yaml")
configure_file(${CMAKE_SOURCE_DIR}/src/generated/config_paths.h.in
               ${CMAKE_BINARY_DIR}/generated/config_paths.h)
```

If `config_paths.h.in` doesn't exist, create it:
```cpp
#pragma once
#define DC_DEFAULT_KEYMAP_PATH "@DEFAULT_KEYMAP_PATH@"
```

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"vim/KeySequence.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run `scripts/verify.sh` before declaring task complete
