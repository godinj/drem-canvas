# fix-vis-scan — Agent Prompts

## Summary

| # | Name | Tier | Dependencies | Files Modified | Files Created |
|---|------|------|-------------|----------------|---------------|
| 01 | BrowserWidget observability | 1 | None | `src/ui/browser/BrowserWidget.{h,cpp}` | — |
| 02 | AppController browser accessor | 1 | None | `src/ui/AppController.h` | — |
| 03 | Async scan e2e harness | 2 | 01, 02 | `src/Main.cpp`, `tests/CMakeLists.txt` | `tests/e2e/test_browser_async_scan.sh` |

## Execution Order

Tier 1 prompts have no file overlap and can run fully in parallel.
Tier 2 depends on both Tier 1 outputs.

```bash
# Tier 1 (parallel — no dependencies, no file overlap)
claude --agent docs/fix-vis-scan/prompts/01-browser-widget-observability.md
claude --agent docs/fix-vis-scan/prompts/02-appcontroller-accessor.md

# Tier 2 (after Tier 1 merges)
claude --agent docs/fix-vis-scan/prompts/03-async-scan-e2e-harness.md
```

## Dependency Graph

```
01-browser-widget-observability ─┐
                                 ├─→ 03-async-scan-e2e-harness
02-appcontroller-accessor ───────┘
```

## Verification

After all agents complete:

```bash
cmake --build --preset release
tests/e2e/test_browser_async_scan.sh ./build/DremCanvas
scripts/verify.sh
```
