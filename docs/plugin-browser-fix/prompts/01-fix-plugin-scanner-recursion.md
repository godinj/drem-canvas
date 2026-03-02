# Agent: Fix Plugin Scanner Recursive Directory Traversal

You are working on the `master` branch of Drem Canvas, a C++17 DAW with Skia GPU rendering.
Your task is to fix a bug where `PluginScanner::findBundles()` only searches the top level of
VST3 search directories, missing plugins nested in subdirectories.

## Context

Read these files before starting:
- `src/dc/plugins/PluginScanner.cpp` (the `findBundles()` method at line 167-188 — this is the bug)
- `src/dc/plugins/PluginScanner.h` (public/private API surface)
- `src/dc/plugins/VST3Host.cpp` (yabridge special-case handling in `getOrLoadModule()`)
- `src/dc/plugins/VST3Module.h` (`isYabridgeBundle()` static method)

## Problem

`PluginScanner::findBundles()` uses `std::filesystem::directory_iterator` which only lists
immediate children of each search directory. VST3 bundles nested in subdirectories are missed.

**Affected layouts:**
- `~/.vst3/yabridge/Kilohearts/*.vst3` — 45 plugins, 2 levels deep (never found)
- `/usr/lib/vst3/Vital.vst3` — 1 plugin, direct child (should be found, verify)

The scanner reports `found 0 VST3 bundle(s) to scan` because all yabridge plugins are nested.

## Deliverables

### 1. Fix `findBundles()` in `src/dc/plugins/PluginScanner.cpp`

Replace `std::filesystem::directory_iterator` with `std::filesystem::recursive_directory_iterator`.

**Critical detail:** `.vst3` bundles are directories themselves (they contain `Contents/`).
The recursive iterator must NOT descend *into* `.vst3` bundles — only find them. Use
`directory_options::follow_directory_symlink` and skip recursion into entries that match
`.vst3` extension by calling `disable_recursion_pending()` on the iterator when a `.vst3`
directory is found.

Implementation:

```cpp
std::vector<std::filesystem::path> PluginScanner::findBundles(
    const std::filesystem::path& searchDir)
{
    std::vector<std::filesystem::path> bundles;
    std::error_code ec;

    auto options = std::filesystem::directory_options::follow_directory_symlink
                 | std::filesystem::directory_options::skip_permission_denied;

    for (auto it = std::filesystem::recursive_directory_iterator(searchDir, options, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
        {
            dc_log("PluginScanner: error iterating: %s", ec.message().c_str());
            ec.clear();
            continue;
        }

        if (it->is_directory() && hasVst3Extension(it->path()))
        {
            bundles.push_back(it->path());
            it.disable_recursion_pending();  // don't descend into the .vst3 bundle
        }
    }

    std::sort(bundles.begin(), bundles.end());
    return bundles;
}
```

### 2. Update header comment in `src/dc/plugins/PluginScanner.h`

Change the `findBundles` doc comment from:
```
/// Enumerate .vst3 bundles in a directory (non-recursive into bundles).
```
to:
```
/// Recursively enumerate .vst3 bundles under a directory.
/// Does not descend into .vst3 bundle directories themselves.
```

### 3. Add unit test `tests/unit/test_plugin_scanner.cpp`

Create a Catch2 test that:

1. Creates a temporary directory tree:
   ```
   tmp/
     direct.vst3/Contents/      (direct child)
     subdir/nested.vst3/Contents/ (nested 1 level)
     subdir/deep/deeper.vst3/Contents/ (nested 2 levels)
     not-a-plugin/              (no .vst3 extension)
   ```
2. Calls `PluginScanner::findBundles()` on `tmp/`
3. Asserts all three `.vst3` bundles are found
4. Asserts `not-a-plugin` is NOT in the results
5. Asserts results are sorted

Add the new test file to `CMakeLists.txt` `target_sources`.

### 4. Manual verification

After building, launch the app and click "Scan Plugins". The log output should show:
```
[DC] PluginScanner: found 46 VST3 bundle(s) to scan
```
(or similar non-zero count matching installed plugins)

## Scope Limitation

Do NOT modify:
- `VST3Host.cpp` — the yabridge handling there is for *loading*, not scanning
- `BrowserWidget.cpp` — the UI trigger is working correctly
- `PluginManager.cpp` — the scan→save flow is correct
- `scanAll()`, `scanOne()`, `scanOneForked()` — these are correct; only `findBundles()` is broken

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
- Run tests: `cmake --build --preset test && ctest --test-dir build-debug --output-on-failure`
- Run `scripts/verify.sh` before declaring task complete
