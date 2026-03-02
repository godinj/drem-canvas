# Fix Empty Plugin List — Agent Prompts

## Overview

| # | Name | Tier | Dependencies | Files Created | Files Modified |
|---|------|------|-------------|--------------|----------------|
| 01 | Auto-scan on empty plugin list | 1 | 05 | `tests/unit/test_auto_scan_trigger.cpp` | `src/ui/AppController.cpp` |
| 02 | In-process yabridge scanning | 1 | — | `tests/unit/test_yabridge_scan.cpp` | `src/dc/plugins/PluginScanner.h`, `src/dc/plugins/PluginScanner.cpp`, `src/dc/plugins/VST3Host.cpp` |
| 03 | ProbeCache-aware scan filtering | 2 | 02 | `tests/unit/test_probecache_scan_filter.cpp` | `src/dc/plugins/PluginScanner.h`, `src/dc/plugins/PluginScanner.cpp`, `src/dc/plugins/VST3Host.cpp` |
| 04 | Background scan with progress UI | 2 | 01, 02 | `tests/unit/test_async_scan.cpp` | `src/plugins/PluginManager.h`, `src/plugins/PluginManager.cpp`, `src/ui/browser/BrowserWidget.h`, `src/ui/browser/BrowserWidget.cpp`, `src/ui/AppController.cpp` |
| 05 | E2E auto-scan test | 0 | — | `tests/e2e/test_auto_scan.sh` | `src/Main.cpp`, `tests/CMakeLists.txt` |

## Dependency Graph

```
05 (e2e test infra) ──> 01 (auto-scan) ─────────────┐
                                                      ├──> 04 (background scan + progress UI)
                        02 (yabridge scan) ──> 03 ──┘
                           (in-process)        (probecache filtering)
```

## Execution Order

### Tier 0 (first — test infrastructure)

```bash
claude --agent docs/fix-plugin-list-empty/prompts/05-e2e-auto-scan-test.md
```

### Tier 1 (after Tier 0 merge — parallel)

```bash
claude --agent docs/fix-plugin-list-empty/prompts/01-auto-scan-on-empty.md
claude --agent docs/fix-plugin-list-empty/prompts/02-yabridge-in-process-scan.md
```

### Tier 2 (after Tier 1 merges)

```bash
# Depends on 02
claude --agent docs/fix-plugin-list-empty/prompts/03-probecache-aware-scanning.md

# Depends on 01 and 02
claude --agent docs/fix-plugin-list-empty/prompts/04-background-scan-progress.md
```

## Merge Order

1. Merge 05 (test infrastructure only — no feature code, no conflicts)
2. Merge 01 and 02 (independent, no conflicts expected)
3. Merge 03 (touches same files as 02 — resolve scanner.h/cpp conflicts)
4. Merge 04 (touches AppController.cpp like 01 — resolve conflict at init block)

## E2E Verification During Implementation

The `test_auto_scan.sh` E2E test (Agent 05) acts as the acceptance test for the
entire fix. It verifies the user-visible behavior: plugins appear at startup without
manual intervention.

- **After merging 05 alone**: `test_auto_scan.sh` should FAIL (expected — auto-scan not implemented yet)
- **After merging 05 + 01**: `test_auto_scan.sh` should PASS (auto-scan triggers on empty list)
- **After merging all**: `test_auto_scan.sh` must still PASS (regression guard)

Agents 01 and 04 reference this test in their verification sections.

## Final Verification

After all merges, run the full verification suite:

```bash
scripts/verify.sh
```

Then launch the app and confirm:
1. Plugins appear in the browser without clicking "Scan Plugins"
2. Yabridge plugins (Phase Plant, kHs Gate) are listed alongside native plugins (Vital)
3. Blocked plugins (kHs Gain) do not appear
4. Clicking "Scan Plugins" shows progress and doesn't freeze the UI
