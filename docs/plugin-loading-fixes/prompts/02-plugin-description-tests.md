# Agent: PluginDescription Unit Tests

You are working on the `master` branch of Drem Canvas, a C++17 DAW with VST3 plugin hosting.
Your task is writing unit tests for `PluginDescription` — the struct that serializes
plugin metadata and converts between VST3 UIDs and hex strings.

## Context

Read these before starting:
- `docs/plugin-loading-fixes/00-prd.md` (Issues 2, 3: UID mapping and legacy format)
- `src/dc/plugins/PluginDescription.h` (full struct definition — `toMap`, `fromMap`, `uidToHexString`, `hexStringToUid`)
- `src/plugins/PluginHost.cpp` (`descriptionFromPropertyTree` at line 47 — currently sets `desc.uid` to file path)
- `tests/unit/foundation/test_string_utils.cpp` (example of existing test style)

## Deliverables

### New files

#### 1. `tests/unit/plugins/test_plugin_description.cpp`

Catch2 test file covering PluginDescription serialization and UID conversion.

**Required test cases:**

- **hexStringToUid valid lowercase**: Pass a 32-char lowercase hex string (e.g.
  `"0123456789abcdef0123456789abcdef"`). Verify returns true and `uid[0..15]` contain
  the correct byte values.

- **hexStringToUid valid uppercase**: Same but with uppercase hex chars. Verify it
  accepts both cases.

- **hexStringToUid valid mixed case**: Mix of upper and lower hex digits.

- **hexStringToUid wrong length**: Pass strings of length 0, 16, 31, 33, 64.
  Verify all return false.

- **hexStringToUid non-hex chars**: Pass a 32-char string with 'g', 'z', '-', ' '.
  Verify returns false.

- **hexStringToUid file path**: Pass a typical file path like
  `"/usr/lib/vst3/MyPlugin.vst3"`. Verify returns false (length != 32 and contains
  non-hex chars). This is the actual bug — `descriptionFromPropertyTree` passes a
  file path as the UID.

- **uidToHexString round-trip**: Generate a 16-byte UID, convert to hex string via
  `uidToHexString`, convert back via `hexStringToUid`. Verify the bytes match.

- **uidToHexString produces lowercase**: Convert a known UID. Verify the output
  contains only `[0-9a-f]` (no uppercase).

- **toMap / fromMap round-trip**: Create a PluginDescription with all fields populated.
  Call `toMap()`, then `fromMap()` on the result. Verify all fields match the original.

- **fromMap missing keys**: Call `fromMap()` with an empty map. Verify no crash,
  all fields have default values (empty strings, 0, false).

- **fromMap partial keys**: Map with only `name` and `path`. Verify those are set,
  rest are defaults.

- **toMap integer fields**: Verify `numInputChannels`, `numOutputChannels`, `hasEditor`,
  `acceptsMidi`, `producesMidi` survive the string round-trip correctly, including edge
  values (0, max int, true/false).

### Modified files

#### 2. `tests/CMakeLists.txt`

Add `unit/plugins/test_plugin_description.cpp` to the `dc_unit_tests` target sources.
PluginDescription is header-only, so no extra library linkage is needed beyond what
the test executable already links.

## Scope Limitation

Only test PluginDescription. Do not fix `descriptionFromPropertyTree` — that's in
prompt 05. Do not test ProbeCache, VST3Module, or PluginInstance.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Test file naming: `test_<class_name>.cpp`
- Use Catch2 `TEST_CASE` with descriptive string names
- Use `SECTION` blocks for shared setup within a test case
- Tag tests with `[plugin_description]`
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Test verification: `./build-debug/dc_unit_tests "[plugin_description]"`
