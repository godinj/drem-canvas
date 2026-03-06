# fix-spatial-scan — Agent Prompts

## Summary

| # | Name | Tier | Dependencies | Files Modified | Files Created |
|---|------|------|-------------|----------------|---------------|
| 01 | X11 child window discovery | 1 | None | `src/platform/linux/X11Reparent.{h,cpp}`, `src/platform/linux/EmbeddedPluginEditor.cpp` | — |
| 02 | Spatial scan diagnostics | 1 | None | `src/ui/pluginview/PluginViewWidget.{h,cpp}`, `src/platform/linux/X11PluginEditorBridge.cpp` | — |
| 03 | E2E compositor validation | 2 | 01 | `src/ui/pluginview/PluginViewWidget.{h,cpp}`, `src/Main.cpp`, `tests/CMakeLists.txt` | `tests/e2e/test_scan_compositor.sh` |

## Root Cause

The compositor captures the GLFW parent X11 window (Vulkan surface → blank X11 pixmap) instead of the plugin's actual child window. `EmbeddedPluginEditor.cpp:63` has a TODO for this. The fix is to discover the child window via `XQueryTree` after `IPlugView::attached()`.

## Execution Order

Tier 1 prompts modify different files and can run fully in parallel.
Tier 2 depends on Tier 1 for the compositor fix to be in place.

```bash
# Tier 1 (parallel — no file overlap)
claude --agent docs/fix-spatial-scan/prompts/01-x11-child-window-discovery.md
claude --agent docs/fix-spatial-scan/prompts/02-spatial-scan-diagnostics.md

# Tier 2 (after Tier 1 merges)
claude --agent docs/fix-spatial-scan/prompts/03-e2e-compositor-validation.md
```

## Dependency Graph

```
01-x11-child-window-discovery ──┐
                                ├─→ 03-e2e-compositor-validation
02-spatial-scan-diagnostics ────┘
```

## Verification

After all agents complete:

```bash
cmake --build --preset release
tests/e2e/test_scan_compositor.sh ./build/DremCanvas
scripts/verify.sh
```
