# Agent: VST3Module Unit Tests

You are working on the `master` branch of Drem Canvas, a C++17 DAW with VST3 plugin hosting.
Your task is writing unit tests for `VST3Module` — specifically the path resolution
helper and the yabridge bundle detection heuristic. These tests use mock directory
structures (no actual plugin binaries needed).

## Context

Read these before starting:
- `docs/plugin-loading-fixes/00-prd.md` (Issues 5, 6: entry point variants, yabridge detection)
- `src/dc/plugins/VST3Module.h` (public API — `load`, `probeModuleSafe`, `isYabridgeBundle`, typedefs)
- `src/dc/plugins/VST3Module.cpp` (implementation — `resolveLibraryPath` anonymous namespace function, `isYabridgeBundle`)
- `tests/unit/foundation/test_base64.cpp` (example of existing test style)

## Deliverables

### New files

#### 1. `tests/unit/plugins/test_vst3_module.cpp`

Catch2 test file covering the testable parts of VST3Module without real plugins.

**Required test cases:**

**isYabridgeBundle tests:**

- **Native Linux plugin**: Create a mock `.vst3` bundle directory with only
  `Contents/x86_64-linux/`. Verify `isYabridgeBundle()` returns false.

- **Yabridge plugin**: Create a mock `.vst3` bundle with both `Contents/x86_64-linux/`
  and `Contents/x86_64-win/`. Verify `isYabridgeBundle()` returns true.

- **macOS-only plugin**: Create a mock `.vst3` bundle with only `Contents/MacOS/`.
  Verify `isYabridgeBundle()` returns false.

- **Non-existent bundle**: Pass a path that doesn't exist. Verify `isYabridgeBundle()`
  returns false (no crash).

- **Empty bundle**: Create an empty `.vst3` directory (no `Contents/`). Verify returns false.

- **Yabridge without linux dir**: Create a bundle with `Contents/x86_64-win/` but no
  `Contents/x86_64-linux/`. Verify returns true (the heuristic checks for x86_64-win only).

**resolveLibraryPath tests:**

Note: `resolveLibraryPath()` is in an anonymous namespace inside VST3Module.cpp.
To test it, you have two options:
1. Test it indirectly via `VST3Module::load()` (which returns nullptr for non-existent paths)
2. Or extract it to a testable location

Since this is a unit test prompt, test indirectly:

- **load with non-existent bundle**: Call `VST3Module::load("/nonexistent/path.vst3")`.
  Verify it returns nullptr and doesn't crash.

- **load with bundle but no .so**: Create a `.vst3` bundle directory structure
  (`Contents/x86_64-linux/`) but don't put a `.so` file inside. Call `VST3Module::load()`.
  Verify it returns nullptr (library not found).

- **load with skipProbe=true on empty bundle**: Same as above but with `skipProbe=true`.
  Verify it still returns nullptr (dlopen fails on missing file).

**probeModuleSafe tests:**

- **probe non-existent file**: Call `probeModuleSafe()` with a path to a non-existent
  bundle. Verify it returns false.

- **probe empty bundle**: Create a `.vst3` bundle with proper structure but no `.so`.
  Verify `probeModuleSafe()` returns false.

**Test helper:**

```cpp
#include <filesystem>

struct TempVST3Bundle
{
    std::filesystem::path base;
    std::filesystem::path bundle;

    TempVST3Bundle (const std::string& name, bool withLinux = true, bool withWin = false)
        : base (std::filesystem::temp_directory_path() / "dc_test_vst3module")
    {
        bundle = base / (name + ".vst3");

        if (withLinux)
            std::filesystem::create_directories (bundle / "Contents" / "x86_64-linux");
        if (withWin)
            std::filesystem::create_directories (bundle / "Contents" / "x86_64-win");
        if (! withLinux && ! withWin)
            std::filesystem::create_directories (bundle);
    }

    ~TempVST3Bundle()
    {
        std::error_code ec;
        std::filesystem::remove_all (base, ec);
    }
};
```

### Modified files

#### 2. `tests/CMakeLists.txt`

Add `unit/plugins/test_vst3_module.cpp` to the `dc_unit_tests` target sources.
This test needs to link against whatever provides `VST3Module` — at minimum
the VST3Module.cpp source and its dependencies (dl, pthread, VST3 SDK headers).

## Scope Limitation

Only test VST3Module's static/utility methods and error paths. Do not attempt to load
actual VST3 plugins — those are integration tests. Do not test ProbeCache,
PluginDescription, or PluginInstance.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Test file naming: `test_<class_name>.cpp`
- Use Catch2 `TEST_CASE` with descriptive string names
- Use `SECTION` blocks for shared setup within a test case
- Tag tests with `[vst3_module]`
- Clean up temp directories in destructors or RAII guards
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Test verification: `./build-debug/dc_unit_tests "[vst3_module]"`
