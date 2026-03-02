# Agent: ProbeCache Unit Tests

You are working on the `master` branch of Drem Canvas, a C++17 DAW with VST3 plugin hosting.
Your task is writing comprehensive unit tests for the `ProbeCache` class — a persistent
cache of VST3 module probe results with dead-man's-pedal crash recovery.

## Context

Read these before starting:
- `docs/plugin-loading-fixes/00-prd.md` (Issue 4: ProbeCache has no test coverage)
- `src/dc/plugins/ProbeCache.h` (full public API — Status enum, load/save, getStatus/setStatus, pedal methods)
- `src/dc/plugins/ProbeCache.cpp` (implementation — YAML format, mtime logic, pedal file I/O)
- `src/dc/foundation/file_utils.h` (`readFileToString`, `writeStringToFile` — used by ProbeCache)
- `tests/unit/foundation/test_base64.cpp` (example of existing test style with Catch2)

## Deliverables

### New files

#### 1. `tests/unit/plugins/test_probe_cache.cpp`

Catch2 test file covering all ProbeCache code paths. Use a temporary directory
(`std::filesystem::temp_directory_path() / "dc_test_probe_cache_XXXXXX"`) for each
TEST_CASE so tests don't interfere with each other or the real cache.

**Required test cases:**

- **YAML round-trip**: Create a ProbeCache, set several entries (safe, blocked), save,
  create a new ProbeCache on the same directory, load, verify all entries match.

- **Mtime invalidation**: Set a bundle as `safe`, save. Touch the bundle file to change
  its mtime. Load a new ProbeCache. Verify `getStatus()` returns `unknown` (stale entry).

- **Dead-man's-pedal recovery**: Write a pedal file manually (simulating a crash mid-load).
  Create a new ProbeCache and call `load()`. Verify the pedal bundle is now `blocked`.
  Verify the pedal file has been cleaned up.

- **setPedal / clearPedal round-trip**: Call `setPedal(path)`, verify pedal file exists
  and contains the path. Call `clearPedal()`, verify pedal file is gone.

- **Missing cache file**: Create ProbeCache on a directory with no `probeCache.yaml`.
  Call `load()`. Verify no crash, all statuses are `unknown`.

- **Corrupted YAML**: Write garbage to `probeCache.yaml`. Call `load()`. Verify no crash,
  entries are empty.

- **Missing cache directory**: Create ProbeCache pointing to a non-existent directory.
  Call `save()`. Verify the directory and file are created.

- **Status transitions**:
  - `unknown` → `safe` → verify `getStatus()` returns `safe`
  - `unknown` → `blocked` → verify `getStatus()` returns `blocked`
  - `safe` → `unknown` (via `setStatus`) → verify `getStatus()` returns `unknown`

- **Multiple entries**: Set status for 5+ different bundle paths. Save, reload, verify
  all entries survive the round-trip independently.

**Test helpers you'll need:**

```cpp
// Create a temp directory for the test
std::filesystem::path createTempDir()
{
    auto base = std::filesystem::temp_directory_path() / "dc_test_probe_cache";
    std::filesystem::create_directories (base);
    return base;
}

// Create a dummy .vst3 bundle directory with a file inside (so it has an mtime)
std::filesystem::path createDummyBundle (const std::filesystem::path& dir, const std::string& name)
{
    auto bundle = dir / (name + ".vst3");
    std::filesystem::create_directories (bundle / "Contents" / "x86_64-linux");
    dc::writeStringToFile (bundle / "Contents" / "x86_64-linux" / (name + ".so"), "dummy");
    return bundle;
}
```

### Modified files

#### 2. `tests/CMakeLists.txt`

Add `unit/plugins/test_probe_cache.cpp` to the `dc_unit_tests` target sources.
The test links against `dc_plugins` (or whatever target provides ProbeCache).
If no `dc_plugins` library target exists yet, link the test source directly
alongside the ProbeCache.cpp source and required dependencies (yaml-cpp, dc_foundation).

## Scope Limitation

Only test ProbeCache. Do not modify ProbeCache itself unless you discover a bug
(in which case, fix it and add a regression test). Do not test VST3Module, PluginInstance,
or VST3Host.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Test file naming: `test_<class_name>.cpp`
- Use Catch2 `TEST_CASE` with descriptive string names
- Use `SECTION` blocks for shared setup within a test case
- Clean up temp directories in each TEST_CASE (use RAII or explicit cleanup)
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Test verification: `./build-debug/dc_unit_tests "[probe_cache]"` (tag your tests)
