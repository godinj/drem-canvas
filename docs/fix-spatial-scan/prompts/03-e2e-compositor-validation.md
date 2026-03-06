# Agent: E2E Compositor + Overlay Validation

You are working on the `feature/fix-vis-scan` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is extending the e2e scan test infrastructure to validate that the compositor is active and capturing pixels when a plugin editor is open.

## Context

Read these specs before starting:
- `CLAUDE.md` (build commands, test commands, conventions)
- `src/Main.cpp` (the `--scan-plugin` code path, around lines 267-347 and 723-795 — two copies exist for macOS/Linux)
- `src/ui/pluginview/PluginViewWidget.h` (public API: `hasSpatialHints()`, `getSpatialResults()`)
- `src/plugins/PluginEditorBridge.h` (`isCompositing()`, `capture()`)
- `tests/e2e/test_scan_cold.sh` (existing spatial scan e2e test — model for new test)
- `tests/CMakeLists.txt` (existing e2e test registration)

## Dependencies

This agent depends on Agent 01 (X11 child window discovery). If the child window fix
is not yet merged, the compositor validation test may fail on X11. The test should still
be structurally correct — it validates the compositor contract regardless of the fix state.

## Deliverables

### Modified files

#### 1. `src/ui/pluginview/PluginViewWidget.h`

Add a public query method for e2e testing:

```cpp
/** Returns true if the editor bridge is active and compositor is capturing pixels. */
bool isCompositorActive() const;
```

#### 2. `src/ui/pluginview/PluginViewWidget.cpp`

Implement:

```cpp
bool PluginViewWidget::isCompositorActive() const
{
    return editorBridge && editorBridge->isOpen() && editorBridge->isCompositing();
}
```

#### 3. `src/Main.cpp`

In the `--scan-plugin` code path (both the macOS and Linux copies), after the existing scan completion check and before the exit, add a compositor status check.

Find the block after `scanDone` is confirmed true (around line 330 / line 788), and add:

```cpp
// Check compositor status
bool compositorOk = pluginViewWidget->isCompositorActive();
std::cerr << "INFO: compositor active=" << (compositorOk ? "true" : "false") << "\n";

if (expectCompositor && ! compositorOk)
{
    std::cerr << "FAIL: compositor not active (expected compositing)\n";
    exitCode = 1;
}
```

Add a new CLI flag `--expect-compositor` that sets `bool expectCompositor = false` to `true` when present. Parse it alongside the existing flags near lines 71-82 and 529-540:

```cpp
else if (arg == "--expect-compositor")
    expectCompositor = true;
```

Declare `bool expectCompositor = false;` alongside the other scan-related booleans.

**Important**: Both the macOS (lines ~260-347) and Linux (lines ~720-795) code paths must be updated identically.

#### 4. `tests/e2e/test_scan_compositor.sh` (new file)

Create a new e2e test script:

```bash
#!/usr/bin/env bash
set -euo pipefail
# E2E: Verify that the X11 compositor captures plugin editor pixels.
#
# Opens a plugin editor via --scan-plugin and checks that:
# 1. Spatial scan completes (hasSpatialHints returns true)
# 2. Compositor is active (isCompositing returns true)
#
# Requires a real plugin and a running X11/XWayland display.

BINARY="${1:?Usage: $0 <binary> [fixture-dir]}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

if [[ ! -x "$BINARY" ]]; then
    echo "SKIP: binary not found: $BINARY"
    exit 0
fi

if [[ ! -d "$FIXTURE" ]]; then
    echo "SKIP: fixture not found: $FIXTURE"
    exit 0
fi

# Need a display for compositor
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "SKIP: no display available"
    exit 0
fi

OUTPUT=$("$BINARY" \
    --load "$FIXTURE" \
    --scan-plugin 0 0 \
    --expect-compositor \
    --exit-after-scan 2>&1) || {
    echo "$OUTPUT"
    echo "FAIL: compositor validation failed"
    exit 1
}

echo "$OUTPUT"

# Verify spatial scan produced results
if ! echo "$OUTPUT" | grep -q "INFO: spatial scan found"; then
    echo "FAIL: spatial scan did not complete"
    exit 1
fi

# Verify compositor was active
if echo "$OUTPUT" | grep -q "FAIL: compositor not active"; then
    echo "FAIL: compositor was not active"
    exit 1
fi

echo "PASS: compositor active with spatial scan results"
```

Make the script executable.

#### 5. `tests/CMakeLists.txt`

Register the new test alongside the existing scan tests (near line 195):

```cmake
add_test(NAME e2e.scan_compositor
    COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_scan_compositor.sh
            $<TARGET_FILE:DremCanvas>
            ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-scan-project)
set_tests_properties(e2e.scan_compositor PROPERTIES
    LABELS "e2e" TIMEOUT 60)
```

### No other files

## Scope Limitation

- Do NOT modify the compositor, scanner, or plugin bridge — only add observability and test infrastructure.
- The `--expect-compositor` flag is opt-in. Existing tests are unaffected.
- The test should `SKIP` (exit 0) gracefully when no display is available or the fixture/binary is missing.

## Conventions

- Namespace: `dc::ui`
- Coding style: spaces around operators, braces on new line for functions, `camelCase` methods
- E2e test scripts use `set -euo pipefail` and `SKIP`/`PASS`/`FAIL` output convention
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources` (no new .cpp in this prompt)
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
