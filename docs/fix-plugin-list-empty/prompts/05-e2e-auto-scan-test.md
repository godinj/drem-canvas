# Agent: E2E Auto-Scan Test

You are working on the `feature/fix-plugin-list-empty` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting. Your task is to add E2E test infrastructure that verifies plugins
are discovered automatically at startup when no `pluginList.yaml` exists — without requiring
the user to manually trigger a scan.

## Context

Read these before starting:
- `docs/fix-plugin-list-empty/design.md` (Root Cause Analysis — "The Gap" section)
- `src/Main.cpp` (both macOS and Linux `--smoke` paths — search for `browserScan` and `expectKnownPluginsGt`)
- `tests/e2e/test_browser_scan.sh` (existing E2E pattern to follow)
- `tests/e2e/e2e_display.sh` (shared display helper)
- `tests/CMakeLists.txt` (E2E test registration block at end of file)

## Problem

The existing `--expect-known-plugins-gt` assertion is only checked inside the
`if (browserScan)` block — it requires `--browser-scan` to trigger an explicit scan
first. There is no way to assert that plugins were discovered by the app's own
startup logic (auto-scan). This means the core fix (Agent 01: auto-scan on empty list)
has no E2E test to verify the user-visible behavior.

## Deliverables

### Migration

#### 1. `src/Main.cpp` — Standalone known-plugins assertion

The `--expect-known-plugins-gt` flag currently only works when paired with `--browser-scan`.
Add a **separate** assertion block that checks the known plugin count after `initialise()`
completes, independently of `--browser-scan`. This tests what auto-scan populated during
normal startup.

Find **both** the macOS and Linux smoke paths. In each, locate the `if (browserScan)` block
and the `// Teardown` comment that follows it. Insert a new assertion block between them:

```cpp
                // Assert known plugins count (auto-scan verification)
                // This checks what initialise() populated — independent of --browser-scan.
                if (! browserScan && expectKnownPluginsGt >= 0)
                {
                    int knownCount = static_cast<int> (
                        appController->getPluginManager().getKnownPlugins().size());
                    std::cerr << "INFO: known plugins after init: " << knownCount << "\n";

                    if (knownCount <= expectKnownPluginsGt)
                    {
                        std::cerr << "FAIL: expected > " << expectKnownPluginsGt
                                  << " known plugins, got " << knownCount << "\n";
                        exitCode = 1;
                    }
                }
```

**macOS path:** Insert between the closing `}` of `if (browserScan)` (around line 342)
and `// Teardown` (around line 344). Adjust indentation to match the surrounding code
(4 levels = 16 spaces).

**Linux path:** Insert between the closing `}` of `if (browserScan)` (around line 676)
and `// Teardown` (around line 678). Adjust indentation to match (2 levels = 8 spaces).

Verify the condition is `! browserScan &&` — this prevents double-checking when
`--browser-scan` is also used (the existing assertion inside `if (browserScan)` already
handles that case).

### New files

#### 2. `tests/e2e/test_auto_scan.sh`

Create a new E2E test script. This is a variant of `test_browser_scan.sh` that does
**not** pass `--browser-scan` — it relies entirely on the app's auto-scan at startup
to populate the plugin list.

```bash
#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"

# Check that at least one VST3 directory exists with plugins
HAS_PLUGINS=false
for dir in /usr/lib/vst3 "$HOME/.vst3"; do
    if [ -d "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
        HAS_PLUGINS=true
        break
    fi
done

if [ "$HAS_PLUGINS" = false ]; then
    echo "SKIP: no VST3 plugins found in standard paths"
    exit 0
fi

# Use isolated app data so there is no pre-existing pluginList.yaml.
# This forces the app to discover plugins from scratch at startup.
export XDG_DATA_HOME="$(mktemp -d)"
trap "rm -rf $XDG_DATA_HOME" EXIT

# Launch WITHOUT --browser-scan. The app should auto-scan because
# pluginList.yaml is empty (fresh XDG_DATA_HOME).
run_with_display 150 "$BINARY" \
    --smoke \
    --expect-known-plugins-gt 0

echo "PASS: auto-scan found plugins at startup (no manual scan trigger)"
```

Make the file executable (`chmod +x`).

#### 3. `tests/CMakeLists.txt` — Register the new E2E test

Add the new test at the end of the `if(BUILD_TESTING)` block, after the existing
`e2e.state_restore` entry:

```cmake
    add_test(NAME e2e.auto_scan
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_auto_scan.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.auto_scan PROPERTIES
        LABELS "e2e" TIMEOUT 150)
```

Use the same 150-second timeout as `e2e.browser_scan` since plugin scanning
(especially yabridge) can be slow.

## Verification

After implementing:

1. **Build:** `cmake --build --preset release` — must compile cleanly
2. **Flag test:** Run the binary with `--smoke --expect-known-plugins-gt -1` (trivially
   passes) to confirm the new assertion code path doesn't crash:
   ```bash
   ./build/DremCanvas_artefacts/Release/DremCanvas --smoke --expect-known-plugins-gt -1
   ```
3. **E2E test (expected to FAIL until Agent 01 is merged):** The test script itself
   should run but is expected to fail with `FAIL: expected > 0 known plugins, got 0`
   because auto-scan doesn't exist yet. Confirm the failure message is correct:
   ```bash
   tests/e2e/test_auto_scan.sh ./build/DremCanvas_artefacts/Release/DremCanvas
   ```
   If it exits non-zero with the expected message, the test infrastructure is working.
4. **Existing tests unaffected:** Run the full test suite and confirm no regressions:
   ```bash
   cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)
   ```

## Scope Limitation

Do NOT implement the auto-scan feature itself (that's Agent 01's job). Only add the
test infrastructure: the Main.cpp assertion path, the shell script, and the CMake
registration. The test is designed to fail until auto-scan is implemented.

Do NOT modify the `if (browserScan)` block or its internal assertion. The existing
`--browser-scan` + `--expect-known-plugins-gt` combination must continue to work as before.

## Conventions

- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Shell scripts use `set -euo pipefail` and source `e2e_display.sh`
- E2E test names follow `test_<feature>.sh` convention
- CMake test names follow `e2e.<feature>` convention
- Build verification: `cmake --build --preset release`
