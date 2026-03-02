# Agent: Browser Scan & Test

You are working on the `feature/e2e-testing` branch of Drem Canvas, a C++17 DAW with
Skia/Vulkan rendering. Your task is adding `--browser-scan` and
`--expect-known-plugins-gt` CLI flags to `Main.cpp`, plus a shell-based E2E test that
opens the plugin browser and scans for installed VST3 plugins.

## Context

Read these specs before starting:
- `src/Main.cpp` (has `--smoke` from Agent 01 — extend the argv parsing)
- `src/ui/AppController.h` (has `getProject()` from Agent 03 — need to add
  `getPluginManager()` getter and make `toggleBrowser()` public)
- `src/ui/AppController.cpp` (`toggleBrowser()` implementation — opens browser widget,
  enters VimEngine PluginMenu mode)
- `src/plugins/PluginManager.h` (`scanForPlugins()`, `getKnownPlugins()`,
  `getDefaultPluginListFile()`, `savePluginList()`)
- `src/ui/browser/BrowserWidget.h` (`refreshPluginList()`, `getNumPlugins()`)

## Dependencies

This agent depends on Agent 03 (load flags). The `--smoke` flag, argv parsing block,
`AppController::getProject()`, and the smoke-mode exit block in `Main.cpp` must already
exist. If they don't, add them yourself following `docs/e2e-testing/prompts/03-load-flags.md`.

## Deliverables

### Modified files

#### 1. `src/Main.cpp`

Extend argv parsing to handle `--browser-scan` and `--expect-known-plugins-gt`.

**Argv parsing (extend existing block):**

```cpp
// Add to existing variables:
bool browserScan = false;
int expectKnownPluginsGt = -1;

// Add to the for loop:
else if (arg == "--browser-scan")
    browserScan = true;
else if (arg == "--expect-known-plugins-gt" && i + 1 < argc)
    expectKnownPluginsGt = std::atoi(argv[++i]);
```

**Browser scan logic in the smoke-mode block:**

After session loading and track/plugin validation (if any), but before teardown, add:

```cpp
// Open browser and scan for plugins if requested
if (browserScan)
{
    appController->toggleBrowser();
    appController->tick();

    appController->getPluginManager().scanForPlugins();

    // Drain ticks to let browser refresh
    for (int i = 0; i < 5; ++i)
        appController->tick();

    int knownCount = static_cast<int>(
        appController->getPluginManager().getKnownPlugins().size());
    std::cerr << "INFO: plugin scan found " << knownCount << " plugins\n";

    if (expectKnownPluginsGt >= 0 && knownCount <= expectKnownPluginsGt)
    {
        std::cerr << "FAIL: expected > " << expectKnownPluginsGt
                  << " known plugins, got " << knownCount << "\n";
        exitCode = 1;
    }

    // Close browser
    appController->toggleBrowser();
    appController->tick();
}
```

Apply the same pattern to the macOS entry path.

#### 2. `src/ui/AppController.h`

Add a public getter for PluginManager and make `toggleBrowser()` public.

```cpp
// In the public section, near getProject():
PluginManager& getPluginManager() { return pluginManager; }
```

Move `toggleBrowser()` from the private section to the public section. Do NOT change
its signature or implementation.

### New files

#### 3. `tests/e2e/test_browser_scan.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

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

# Use isolated app data so scan starts fresh
export XDG_DATA_HOME="$(mktemp -d)"
trap "rm -rf $XDG_DATA_HOME" EXIT

timeout 120 xvfb-run -a "$BINARY" \
    --smoke \
    --browser-scan \
    --expect-known-plugins-gt 0

echo "PASS: browser scan found plugins"
```

Make executable: `chmod +x tests/e2e/test_browser_scan.sh`.

Notes on the test:
- Uses `XDG_DATA_HOME` (not `XDG_CONFIG_HOME`) because `PluginManager` stores its
  plugin list in `~/.local/share/DremCanvas/` via `getUserAppDataDirectory()`.
- Timeout is 120s because `scanForPlugins()` is synchronous and forks a child process
  per `.vst3` bundle — can take 30-60s on a machine with many plugins.
- `--expect-known-plugins-gt 0` means at least 1 plugin must be found.
- The skip guard checks that at least one standard VST3 directory is non-empty.

#### 4. `tests/CMakeLists.txt` (append)

Add after the existing E2E test entries:

```cmake
    add_test(NAME e2e.browser_scan
        COMMAND ${CMAKE_SOURCE_DIR}/tests/e2e/test_browser_scan.sh
                $<TARGET_FILE:DremCanvas>)
    set_tests_properties(e2e.browser_scan PROPERTIES
        LABELS "e2e" TIMEOUT 150)
```

## Scope Limitation

- Do NOT modify `PluginManager`, `VST3Host`, `PluginScanner`, `BrowserWidget`, or any
  plugin infrastructure. Use the existing APIs as-is.
- Do NOT add `--load`, `--scan-plugin`, or other flags from Agents 03/04.
- `scanForPlugins()` is synchronous — no polling or sleep needed. Just call it and
  check the result.
- The `toggleBrowser()` method already exists in `AppController.cpp` — just move its
  declaration from private to public in the header.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Header includes use `<JuceHeader.h>` plus project-relative paths
- Build verification: `cmake --build --preset test`
