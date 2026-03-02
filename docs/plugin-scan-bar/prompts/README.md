# Plugin Scan Progress Bar — Agent Prompts

## Prompt Summary

| # | Name | Tier | Dependencies | Files Created | Files Modified |
|---|------|------|-------------|---------------|----------------|
| 01 | ProgressBarWidget | 1 | None | `src/graphics/widgets/ProgressBarWidget.h`, `ProgressBarWidget.cpp`, `tests/unit/test_ProgressBarWidget.cpp` | `CMakeLists.txt`, `tests/CMakeLists.txt` |
| 02 | Async Plugin Scan | 1 | None | `tests/unit/test_PluginManager_async.cpp` | `src/plugins/PluginManager.h`, `src/plugins/PluginManager.cpp`, `src/ui/AppController.h`, `tests/CMakeLists.txt` |
| 03 | Browser Scan UI | 2 | 01, 02 | None | `src/ui/browser/BrowserWidget.h`, `src/ui/browser/BrowserWidget.cpp` |

## Dependency Graph

```
01-progress-bar-widget ──┐
                         ├──> 03-browser-scan-ui
02-async-plugin-scan ────┘
```

## Execution

```bash
# Tier 1 (parallel)
claude -p docs/plugin-scan-bar/prompts/01-progress-bar-widget.md
claude -p docs/plugin-scan-bar/prompts/02-async-plugin-scan.md

# Tier 2 (after Tier 1 merges)
claude -p docs/plugin-scan-bar/prompts/03-browser-scan-ui.md
```
