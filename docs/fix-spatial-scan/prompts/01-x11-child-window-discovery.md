# Agent: X11 Child Window Discovery

You are working on the `feature/fix-vis-scan` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is fixing the X11 compositor: discover the actual plugin child window instead of passing the GLFW parent window to `XCompositeRedirectWindow`.

## Context

Read these specs before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `docs/plugin-parameter-finder.md` (spatial hint design, coordinate considerations)
- `src/platform/linux/EmbeddedPluginEditor.cpp` (the TODO at line 62-63 — this is the bug)
- `src/platform/linux/X11PluginEditorBridge.cpp` (calls `startRedirect` with the wrong window)
- `src/platform/linux/X11Compositor.cpp` (`startRedirect` / `capture` / `acquirePixmap`)
- `src/platform/linux/X11Reparent.h` (existing X11 helpers — add new ones here)
- `src/platform/linux/X11Reparent.cpp` (implementation of X11 helpers)

## Problem

`EmbeddedPluginEditor::openEditor()` attaches the IPlugView to the GLFW parent X11 window. The plugin creates a child window under it. But `editorXWindow` is set to the **parent** window (line 63):

```cpp
editorXWindow = parentX11Window;  // TODO: discover the actual child window
```

The compositor then calls `XCompositeRedirectWindow` on the GLFW parent. Since GLFW renders via Vulkan (which bypasses X11's rendering pipeline), the X11 pixmap for the parent is blank. `capture()` returns a blank or garbage image. This breaks `isCompositing()` → `computeCompositeGeometry()` → overlay hint rendering.

The fix: after `attachToWindow()`, enumerate children of the parent to find the new child window the IPlugView created, and store *that* as `editorXWindow`.

## Deliverables

### Modified files

#### 1. `src/platform/linux/X11Reparent.h`

Add a new helper function declaration:

```cpp
/** Find the first child window of the given parent.
    Returns 0 if no children exist. Useful for discovering
    the X11 window created by IPlugView::attached(). */
unsigned long findFirstChild (void* display, unsigned long parent);
```

#### 2. `src/platform/linux/X11Reparent.cpp`

Implement `findFirstChild`:

- Call `XQueryTree(display, parent, &root, &parentOut, &children, &nChildren)`
- If `nChildren > 0`, return `children[nChildren - 1]` (last child = most recently created)
- `XFree(children)` after copying the result
- Return 0 if no children found
- Log via `std::cerr` the child count and selected window ID for debugging

#### 3. `src/platform/linux/EmbeddedPluginEditor.cpp`

In `openEditor()`, after `editor_->attachToWindow(parentX11Window)` (the X11 reparented path, around line 55):

1. Call `XSync(display, False)` to ensure the plugin's window creation has been processed by the X server
2. Call `x11::findFirstChild(xDisplay, parentX11Window)` to discover the child
3. If a child is found, set `editorXWindow = child`
4. If no child is found, log a warning and keep `editorXWindow = parentX11Window` as fallback
5. Log the discovered child window ID: `dc_log("[EmbedEditor] discovered plugin child window: %lu (parent=%lu)", editorXWindow, parentX11Window)`

**Important**: The Wayland path (lines 67-87) already creates its own container window and does NOT need this fix. Only modify the X11 path (the `if (parentX11Window != 0)` branch).

#### 4. `src/platform/linux/X11PluginEditorBridge.cpp`

No changes needed — it already calls `embeddedEditor->getXWindow()` which will now return the correct child window.

## Edge Cases

- **Plugin creates no child window**: Some plugins (rare) may render directly into the parent. Keep the parent as fallback.
- **Plugin creates multiple children**: VST3 plugins typically create one top-level child. Use the last child (`children[nChildren - 1]`) as it's the most recently created.
- **Timing**: `XSync` before the query ensures the child window exists. If the plugin creates the window asynchronously (very rare for VST3), the fallback to the parent is acceptable.
- **Wayland path**: Do NOT modify the Wayland path — it already creates a dedicated container window that works correctly with the compositor.

## Conventions

- Namespace: `dc::platform::x11`
- Coding style: spaces around operators, braces on new line for functions, `camelCase` methods
- Header includes use project-relative paths
- X11 headers are only included in `.cpp` files (never in `.h`) to avoid macro conflicts
- The `void*` display / `unsigned long` window pattern keeps X11 types out of public headers
- Build verification: `cmake --build --preset release`
