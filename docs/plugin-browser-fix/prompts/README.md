# Plugin Browser Fix — Agent Prompts

## Prompts

| # | Name | Tier | Dependencies | Files Modified | Files Created |
|---|------|------|-------------|---------------|---------------|
| 01 | fix-plugin-scanner-recursion | 1 | none | `src/dc/plugins/PluginScanner.cpp`, `src/dc/plugins/PluginScanner.h` | `tests/unit/test_plugin_scanner.cpp` |

## Execution

Single-tier task — one agent:

```bash
# From main/ worktree
claude --agent docs/plugin-browser-fix/prompts/01-fix-plugin-scanner-recursion.md
```

## Problem Summary

`PluginScanner::findBundles()` uses shallow `directory_iterator`, missing `.vst3` bundles
nested in subdirectories (e.g., `~/.vst3/yabridge/Kilohearts/*.vst3`). Fix is to switch
to `recursive_directory_iterator` with `disable_recursion_pending()` to avoid descending
into `.vst3` bundle directories.
