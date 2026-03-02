# Tier 1: App Launch & Clean Exit

## Goal

Verify the binary starts, initialises all subsystems, and exits cleanly.

## The problem

`Main.cpp` creates a GLFW window + Vulkan backend and enters a render loop. There is
no headless mode, no `--quit-after-init` flag, and no CLI batch mode. The test needs a
way to tell the app to exit after initialisation completes.

## Execution plan

### Step 1 — Add a `--smoke` flag to `Main.cpp`

Parse `argv` for `--smoke`. When present:

- Run `appController->initialise()` normally.
- Call `appController->tick()` once (processes message queue, proves the loop works).
- Skip the render loop entirely (or run exactly 1 frame).
- Tear down and `return 0`.

This is ~10 lines in the Linux `main()` and ~10 in the macOS `main()`. The flag is
invisible to normal usage.

### Step 2 — Require a virtual framebuffer (Linux CI)

The Vulkan backend needs a display. Options:

- **Xvfb** (`xvfb-run ./build/DremCanvas --smoke`) — standard, works with GLFW/X11.
- **SwiftShader** (software Vulkan ICD) — if the CI box has no GPU, point
  `VK_ICD_FILENAMES` to SwiftShader's `vk_swiftshader_icd.json`.

Xvfb alone works if the machine has a GPU or SwiftShader installed. The dev machine
already has Vulkan (`libvulkan-dev`), so `xvfb-run` is likely sufficient.

### Step 3 — Write a shell-based E2E test

```bash
# tests/e2e/test_smoke.sh
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
timeout 15 xvfb-run -a "$BINARY" --smoke
echo "PASS: smoke test (clean launch + exit)"
```

Exit code 0 = pass, non-zero = crash/hang. The `timeout` catches infinite loops.

### Step 4 — CMake integration

Add to `tests/CMakeLists.txt`:

```cmake
if(BUILD_TESTING)
    add_test(NAME e2e.smoke
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_smoke.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.smoke PROPERTIES
        LABELS "e2e" TIMEOUT 30)
endif()
```

## What this validates

- CMake build produces a working binary.
- GLFW window creation doesn't segfault.
- Vulkan/Skia backend initialises.
- AudioEngine, MidiEngine, PluginManager all construct without crashing.
- `AppController::initialise()` completes (audio graph, vim engine, UI widgets).
- Clean teardown in reverse order (no double-free, no dangling threads).
