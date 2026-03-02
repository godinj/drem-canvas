# Agent: Smoke Flag & Test

You are working on the `feature/e2e-testing` branch of Drem Canvas, a C++17 DAW with
Skia/Vulkan rendering. Your task is adding a `--smoke` CLI flag that exits after
initialisation, plus a shell-based E2E smoke test.

## Context

Read these specs before starting:
- `docs/e2e-testing/01-tier1-smoke.md` (full execution plan for `--smoke`)
- `docs/e2e-testing/04-implementation-plan.md` (Phases A + B)
- `src/Main.cpp` (current Linux + macOS entry points — no argv parsing exists yet)

## Deliverables

### Modified files

#### 1. `src/Main.cpp`

Add `--smoke` argv parsing to both the Linux `main()` and the macOS entry path.

**Linux `main()` (starts around line 125):**

The current structure is:
```cpp
int main(int argc, char* argv[])
{
    auto glfwWindow = ...;
    auto gpuBackend = ...;
    auto renderer = ...;
    auto appController = std::make_unique<dc::ui::AppController>();
    // ... wire callbacks ...
    appController->initialise();
    glfwWindow->show();

    while (!shouldQuit && !glfwWindow->shouldClose())
    {
        glfwWindow->pollEvents();
        appController->tick();
        renderer->renderFrame(*appController);
    }
    // teardown
    return 0;
}
```

Changes:
- At the top of `main()`, parse `argv` for `--smoke`:
  ```cpp
  bool smokeMode = false;
  for (int i = 1; i < argc; ++i)
  {
      if (std::string(argv[i]) == "--smoke")
          smokeMode = true;
  }
  ```
- After `appController->initialise()` and `glfwWindow->show()`, if `smokeMode` is true:
  ```cpp
  if (smokeMode)
  {
      appController->tick();  // one tick to prove the loop works
      appController.reset();
      eventDispatch.reset();
      renderer.reset();
      gpuBackend.reset();
      glfwWindow.reset();
      return 0;
  }
  ```
- The normal render loop follows unchanged for non-smoke mode.

**macOS entry path:**

The macOS path uses `dc_runNSApplication()` with lambdas for `applicationDidFinishLaunching`
and `applicationWillTerminate`. Apply the same pattern:
- Parse `argv` for `--smoke` before calling `dc_runNSApplication()`.
- In the `applicationDidFinishLaunching` lambda, after `appController->initialise()`,
  if `smokeMode`, call `tick()` once and then request app termination
  (e.g., call the native terminate or set a flag that the frame callback checks).

### New files

#### 2. `tests/e2e/test_smoke.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
timeout 15 xvfb-run -a "$BINARY" --smoke
echo "PASS: smoke test (clean launch + exit)"
```

Make executable: `chmod +x tests/e2e/test_smoke.sh`.

#### 3. `tests/CMakeLists.txt` (append)

Add after the existing `catch_discover_tests()` blocks:

```cmake
# --- E2E tests (shell-based, exercise real binary) ---
if(BUILD_TESTING)
    add_test(NAME e2e.smoke
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_smoke.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.smoke PROPERTIES
        LABELS "e2e" TIMEOUT 30)
endif()
```

## Scope Limitation

- Do NOT add `--load`, `--expect-tracks`, `--expect-plugins`, or any other flags.
  Only `--smoke`.
- Do NOT modify `AppController.h` or `AppController.cpp`. The smoke path only calls
  the existing `initialise()` and `tick()` public methods.
- Do NOT create fixture YAML files.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
