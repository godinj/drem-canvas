# fix-scan-ui — Agent Prompts

## Summary

| # | Name | Tier | Dependencies | Files Modified |
|---|------|------|-------------|----------------|
| 01 | Yabridge scan serialization | 1 | None | `src/dc/plugins/PluginScanner.{h,cpp}` |
| 02 | Progress bar wiring | 1 | None | `src/ui/browser/BrowserWidget.{h,cpp}` |

## Execution Order

Both prompts are Tier 1 with no file overlap — they can run fully in parallel.

```bash
# Tier 1 (parallel — no dependencies, no file overlap)
claude --agent docs/fix-scan-ui/prompts/01-yabridge-scan-serialization.md
claude --agent docs/fix-scan-ui/prompts/02-progress-bar-wiring.md
```

## Dependency Graph

```
01-yabridge-scan-serialization ─┐
                                ├─ (no dependencies between them)
02-progress-bar-wiring ─────────┘
```
