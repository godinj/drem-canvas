# Agent: Load Flags & Test

You are working on the `feature/e2e-testing` branch of Drem Canvas, a C++17 DAW with
Skia/Vulkan rendering. Your task is adding `--load`, `--expect-tracks`, and
`--expect-plugins` CLI flags to `Main.cpp`, plus a shell-based E2E test for project
loading with plugins.

## Context

Read these specs before starting:
- `docs/e2e-testing/02-tier2-load-project.md` (full execution plan)
- `docs/e2e-testing/04-implementation-plan.md` (Phases C + E)
- `src/Main.cpp` (has `--smoke` flag from Agent 01 — extend the argv parsing)
- `src/ui/AppController.h` (public interface — need to add `getProject()` getter)
- `src/ui/AppController.cpp` (`loadSessionFromDirectory()` is currently private)
- `src/model/Project.h` (`getNumTracks()`, `getTrack()` for validation)

## Dependencies

This agent depends on Agent 01 (smoke flag). `--smoke` mode must already exist in
`Main.cpp`. If it doesn't, add the `--smoke` parsing yourself following the pattern in
`docs/e2e-testing/prompts/01-smoke-flag.md`.

## Deliverables

### Modified files

#### 1. `src/Main.cpp`

Extend the argv parsing block (added by Agent 01) to also handle `--load`, `--expect-tracks`,
and `--expect-plugins`.

**Argv parsing (extend existing block):**

```cpp
bool smokeMode = false;
std::string loadPath;
int expectTracks = -1;
int expectPlugins = -1;

for (int i = 1; i < argc; ++i)
{
    std::string arg(argv[i]);
    if (arg == "--smoke")
        smokeMode = true;
    else if (arg == "--load" && i + 1 < argc)
        loadPath = argv[++i];
    else if (arg == "--expect-tracks" && i + 1 < argc)
        expectTracks = std::atoi(argv[++i]);
    else if (arg == "--expect-plugins" && i + 1 < argc)
        expectPlugins = std::atoi(argv[++i]);
}
```

**Smoke-mode exit block (extend existing block):**

After `appController->initialise()` and the first `tick()`, if `smokeMode`:

```cpp
if (smokeMode)
{
    // Load a session if requested
    if (!loadPath.empty())
    {
        appController->loadSessionFromDirectory(std::filesystem::path(loadPath));

        // Drain a few ticks — plugins load asynchronously
        for (int i = 0; i < 10; ++i)
            appController->tick();
    }

    int exitCode = 0;

    // Validate track count
    if (expectTracks >= 0)
    {
        int actual = appController->getProject().getNumTracks();
        if (actual != expectTracks)
        {
            std::cerr << "FAIL: expected " << expectTracks << " tracks, got "
                      << actual << "\n";
            exitCode = 1;
        }
    }

    // Validate plugin count
    if (expectPlugins >= 0)
    {
        int totalPlugins = 0;
        auto& project = appController->getProject();
        for (int t = 0; t < project.getNumTracks(); ++t)
        {
            dc::Track track(project.getTrack(t));
            totalPlugins += track.getNumPlugins();
        }
        if (totalPlugins != expectPlugins)
        {
            std::cerr << "FAIL: expected " << expectPlugins << " plugins, got "
                      << totalPlugins << "\n";
            exitCode = 1;
        }
    }

    // Teardown
    appController.reset();
    eventDispatch.reset();
    renderer.reset();
    gpuBackend.reset();
    glfwWindow.reset();
    return exitCode;
}
```

Apply the same pattern to the macOS entry path.

**Required includes** (add if not present):
```cpp
#include <cstdlib>      // std::atoi
#include <filesystem>
#include <iostream>      // std::cerr
#include "model/Track.h" // dc::Track for plugin counting
```

#### 2. `src/ui/AppController.h`

Add a public getter so `Main.cpp` can access the project for validation:

```cpp
// In the public section, near the existing getRenderer()/setRenderer():
Project& getProject() { return project; }
const Project& getProject() const { return project; }
```

Also make `loadSessionFromDirectory` public (it's currently private). Move its
declaration from the private section to the public section. Do NOT change its
signature or implementation — just its access level.

### New files

#### 3. `tests/e2e/test_load_project.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-plugin-project}"

# Check plugin availability — skip if plugins are missing
check_plugin() {
    local path="$1"
    # Expand ~ to $HOME
    path="${path/#\~/$HOME}"
    if [ ! -e "$path" ]; then
        echo "SKIP: plugin not found: $path"
        exit 0
    fi
}

check_plugin "/usr/lib/vst3/Vital.vst3"
check_plugin "~/.vst3/yabridge/Kilohearts/kHs Gain.vst3"

timeout 30 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 2 \
    --expect-plugins 2

echo "PASS: project load with plugins"
```

Make executable: `chmod +x tests/e2e/test_load_project.sh`.

#### 4. `tests/CMakeLists.txt` (append)

Add after the `e2e.smoke` test entry:

```cmake
    add_test(NAME e2e.load_project
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_load_project.sh
                $<TARGET_FILE:DremCanvas>
                ${CMAKE_SOURCE_DIR}/tests/fixtures/e2e-plugin-project)
    set_tests_properties(e2e.load_project PROPERTIES
        LABELS "e2e" TIMEOUT 60)
```

## Scope Limitation

- Do NOT add `--scan-plugin`, `--no-spatial-cache`, or `--expect-spatial-params-gt`.
  Those are Agent 04's scope.
- Do NOT create fixture YAML files — Agent 02 owns those.
- Do NOT modify `AppController.cpp` — only move `loadSessionFromDirectory` from
  private to public in the header.
- Keep the `getProject()` getter minimal — return a reference, no copies.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Header includes use `<JuceHeader.h>` plus project-relative paths
- Build verification: `cmake --build --preset test`
