# E2E Testing — Agent Prompts

## Prompt Summary

| # | Name | Tier | Dependencies | Files Created | Files Modified |
|---|------|------|-------------|---------------|----------------|
| 01 | Smoke Flag & Test | 1 | — | `tests/e2e/test_smoke.sh` | `src/Main.cpp`, `tests/CMakeLists.txt` |
| 02 | Test Fixture Projects | 1 | — | `tests/fixtures/e2e-plugin-project/{session,track-0,track-1}.yaml`, `tests/fixtures/e2e-scan-project/{session,track-0}.yaml` | — |
| 03 | Load Flags & Test | 2 | 01 | `tests/e2e/test_load_project.sh` | `src/Main.cpp`, `src/ui/AppController.h`, `tests/CMakeLists.txt` |
| 04 | Scan Flags & Tests | 3 | 03 | `tests/e2e/test_scan_cold.sh`, `tests/e2e/test_scan_warm.sh` | `src/Main.cpp`, `src/ui/AppController.h`, `tests/CMakeLists.txt` |
| 05 | Browser Scan & Test | 3 | 03 | `tests/e2e/test_browser_scan.sh` | `src/Main.cpp`, `src/ui/AppController.h`, `tests/CMakeLists.txt` |

## Dependency Graph

```
Tier 1 (parallel):  [01 Smoke Flag]   [02 Fixtures]
                          |                  |
Tier 2:              [03 Load Flags] <-------+
                        |         |
Tier 3:       [04 Scan Flags]  [05 Browser Scan]
```

Agents 04 and 05 both depend on 03 and both modify `src/Main.cpp` —
run them sequentially (04 then 05, or vice versa) to avoid merge conflicts.

## Execution Order

```bash
# Tier 1 (parallel)
claude --agent docs/e2e-testing/prompts/01-smoke-flag.md &
claude --agent docs/e2e-testing/prompts/02-test-fixtures.md &
wait

# Tier 2 (after Tier 1 merges)
claude --agent docs/e2e-testing/prompts/03-load-flags.md

# Tier 3 (sequential — both modify Main.cpp)
claude --agent docs/e2e-testing/prompts/04-scan-flags.md
claude --agent docs/e2e-testing/prompts/05-browser-scan.md
```

## Running the E2E tests

After all agents complete:

```bash
cmake --preset test
cmake --build --preset test
ctest --test-dir build-debug -L e2e --output-on-failure
```

Or run individual tests:

```bash
ctest --test-dir build-debug -R e2e.smoke
ctest --test-dir build-debug -R e2e.load_project
ctest --test-dir build-debug -R "e2e.scan_(cold|warm)"
ctest --test-dir build-debug -R e2e.browser_scan
```
