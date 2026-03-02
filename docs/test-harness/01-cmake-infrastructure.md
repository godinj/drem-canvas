# 01 — CMake & Framework Infrastructure

> Extract dc:: sources into separate library targets, integrate Catch2 v3,
> and add CMake presets for testing and coverage.

**Phase**: 1 (CMake Infrastructure)
**Dependencies**: None (first phase)
**Related**: [00-prd.md](00-prd.md), [02-migration-tests.md](02-migration-tests.md),
[03-architecture-guards.md](03-architecture-guards.md)

---

## Overview

The project currently compiles all source files — including the four dc:: libraries —
into a single monolithic `DremCanvas` executable. This means tests cannot link against
dc:: code without also linking the entire application, including JUCE, Skia, and
platform-specific code.

Phase 1 extracts `dc_foundation`, `dc_model`, `dc_midi`, and `dc_audio` into separate
CMake static library targets. Unit tests link only these libraries — never JUCE. This
separation is itself the strongest migration guard: any `#include <JuceHeader.h>` in a
dc:: source file will fail at compile time because the JUCE include paths are not
available to these targets.

---

## Catch2 v3 Integration

### FetchContent Configuration

Add to the top-level `CMakeLists.txt`, inside the `if(BUILD_TESTING)` block:

```cmake
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
)
FetchContent_MakeAvailable(Catch2)

# CRITICAL: Catch2's catch_discover_tests() macro lives in extras/
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
```

### Why Catch2 v3 Over Alternatives

| Feature | Catch2 v3 | GoogleTest | doctest |
|---------|-----------|------------|---------|
| JUCE ecosystem standard | Yes (Pamplejuce) | No | No |
| String literal test names | Yes | No (C++ identifiers) | Yes |
| `SECTION` shared setup | Yes | No (fixtures only) | Yes |
| Built-in benchmarking | `BENCHMARK` macro | No (separate lib) | No |
| CMake auto-registration | `catch_discover_tests()` | `gtest_discover_tests()` | Manual |
| Trompeloeil adapter | Dedicated header | Separate adapter | No |
| Compile speed (v3) | Compiled library | Compiled library | Header-only (~40x faster) |

Catch2 v3 moved from header-only to a compiled library, significantly improving
compile times compared to v2. doctest's compile speed advantage is less relevant now.

### Trompeloeil (Mocking)

```cmake
FetchContent_Declare(
    trompeloeil
    GIT_REPOSITORY https://github.com/rollbear/trompeloeil.git
    GIT_TAG        v49
)
FetchContent_MakeAvailable(trompeloeil)
```

Header-only. Integration with Catch2 via `<catch2/trompeloeil.hpp>`.

### RapidCheck (Property-Based Testing)

```cmake
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        master
    CMAKE_ARGS     -DRC_ENABLE_CATCH=ON
)
FetchContent_MakeAvailable(rapidcheck)
```

Integration with Catch2 via `#include <rapidcheck/catch.h>`.

---

## dc:: Library Targets

### dc_foundation

Pure C++17. No external dependencies (except `<pthread>` on Linux).

```cmake
add_library(dc_foundation STATIC
    src/dc/foundation/base64.cpp
    src/dc/foundation/message_queue.cpp
    src/dc/foundation/worker_thread.cpp
)

target_include_directories(dc_foundation PUBLIC
    src/
    ${CMAKE_BINARY_DIR}/generated/
)

target_compile_features(dc_foundation PUBLIC cxx_std_17)

# Thread support
find_package(Threads REQUIRED)
target_link_libraries(dc_foundation PUBLIC Threads::Threads)
```

### dc_model

Depends only on `dc_foundation`.

```cmake
add_library(dc_model STATIC
    src/dc/model/Variant.cpp
    src/dc/model/PropertyId.cpp
    src/dc/model/PropertyTree.cpp
    src/dc/model/UndoManager.cpp
)

target_link_libraries(dc_model PUBLIC dc_foundation)
```

### dc_midi

Depends only on `dc_foundation`. RtMidi is needed for `MidiDeviceManager` but unit
tests mock the device layer, so it is not linked here.

```cmake
add_library(dc_midi STATIC
    src/dc/midi/MidiMessage.cpp
    src/dc/midi/MidiBuffer.cpp
    src/dc/midi/MidiSequence.cpp
)

target_link_libraries(dc_midi PUBLIC dc_foundation)
```

Note: `MidiDeviceManager.cpp` is excluded from `dc_midi` because it depends on
RtMidi. It remains in the main application target. Tests that need MIDI device
interaction use Trompeloeil mocks.

### dc_audio

Depends on `dc_foundation` plus system libraries (`libsndfile`).

```cmake
add_library(dc_audio STATIC
    src/dc/audio/AudioFileReader.cpp
    src/dc/audio/AudioFileWriter.cpp
    src/dc/audio/DiskStreamer.cpp
    src/dc/audio/ThreadedRecorder.cpp
)

target_link_libraries(dc_audio PUBLIC dc_foundation Threads::Threads)

# libsndfile (required for audio file I/O)
if(UNIX AND NOT APPLE)
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    target_include_directories(dc_audio PRIVATE ${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(dc_audio PRIVATE ${SNDFILE_LIBRARIES})
elseif(APPLE)
    # macOS: use brew-installed libsndfile or FetchContent
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    target_include_directories(dc_audio PRIVATE ${SNDFILE_INCLUDE_DIRS})
    target_link_libraries(dc_audio PRIVATE ${SNDFILE_LIBRARIES})
endif()
```

Note: `PortAudioDeviceManager.cpp` is excluded — it depends on PortAudio and is only
needed by the main application. Tests mock the `AudioDeviceManager` interface.

### Main Application Target

Update the existing `DremCanvas` target to link the dc:: libraries instead of
compiling their sources directly:

```cmake
# Remove dc:: source files from target_sources(DremCanvas ...)
# Add library linkage instead:
target_link_libraries(DremCanvas PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
    # ... existing JUCE, Skia, platform deps
)
```

---

## Test Executables

### tests/CMakeLists.txt

```cmake
# --- Unit Tests (NO JUCE) ---
add_executable(dc_unit_tests
    unit/foundation/test_colour.cpp
    unit/foundation/test_string_utils.cpp
    unit/foundation/test_base64.cpp
    unit/foundation/test_random.cpp
    unit/foundation/test_spsc_queue.cpp
    unit/foundation/test_message_queue.cpp
    unit/foundation/test_listener_list.cpp
    unit/foundation/test_worker_thread.cpp
    unit/model/test_variant.cpp
    unit/model/test_property_id.cpp
    unit/model/test_property_tree.cpp
    unit/model/test_undo_manager.cpp
    unit/midi/test_midi_message.cpp
    unit/midi/test_midi_buffer.cpp
    unit/midi/test_midi_sequence.cpp
    unit/audio/test_audio_block.cpp
    unit/audio/test_audio_file_io.cpp
    unit/audio/test_audio_disk_streamer.cpp
    unit/audio/test_audio_threaded_recorder.cpp
)

target_link_libraries(dc_unit_tests PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
    Catch2::Catch2WithMain
    trompeloeil::trompeloeil
)

catch_discover_tests(dc_unit_tests)

# --- Integration Tests (links JUCE) ---
add_executable(dc_integration_tests
    integration/main.cpp
    integration/test_session_roundtrip.cpp
    integration/test_vim_commands.cpp
    integration/test_audio_graph.cpp
)

target_link_libraries(dc_integration_tests PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
    juce::juce_core
    juce::juce_data_structures
    juce::juce_audio_basics
    yaml-cpp::yaml-cpp
    Catch2::Catch2          # Note: no WithMain (custom main.cpp)
    trompeloeil::trompeloeil
)

catch_discover_tests(dc_integration_tests)
```

### Integration Test Main

The integration test executable needs JUCE initialised before tests run:

```cpp
// tests/integration/main.cpp
#include <JuceHeader.h>
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    return Catch::Session().run(argc, argv);
}
```

---

## CMake Presets

Add to `CMakePresets.json`:

```json
{
    "name": "test",
    "displayName": "Test (Debug + Testing)",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build-debug",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "BUILD_TESTING": "ON"
    }
}
```

```json
{
    "name": "coverage",
    "displayName": "Coverage (Debug + Instrumentation)",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build-coverage",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "BUILD_TESTING": "ON",
        "CMAKE_CXX_FLAGS": "-fprofile-instr-generate -fcoverage-mapping",
        "CMAKE_C_FLAGS": "-fprofile-instr-generate -fcoverage-mapping"
    }
}
```

Add corresponding build presets:

```json
{"name": "test", "configurePreset": "test"},
{"name": "coverage", "configurePreset": "coverage"}
```

---

## Top-Level CMakeLists.txt Changes

```cmake
# After existing project() declaration:
option(BUILD_TESTING "Build test targets" OFF)

# dc:: library targets (always built)
add_library(dc_foundation STATIC ...)
add_library(dc_model STATIC ...)
add_library(dc_midi STATIC ...)
add_library(dc_audio STATIC ...)

# Main application
add_executable(DremCanvas ...)
target_link_libraries(DremCanvas PRIVATE dc_foundation dc_model dc_midi dc_audio ...)

# Test targets (only when BUILD_TESTING=ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
```

---

## CTest Integration

`catch_discover_tests()` automatically registers every `TEST_CASE` as a CTest test.
Use `POST_BUILD` timing (not `POST_CONFIGURE`) so test registration happens after the
executable is built, not when CMake configures:

```cmake
catch_discover_tests(dc_unit_tests
    TEST_PREFIX "unit."
    REPORTER XML
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
```

### Running Tests

```bash
# Configure + build
cmake --preset test
cmake --build --preset test

# Run all tests
ctest --test-dir build-debug --output-on-failure -j$(nproc)

# Run only unit tests
ctest --test-dir build-debug -R "unit\." --output-on-failure

# Run only integration tests
ctest --test-dir build-debug -R "integration\." --output-on-failure

# Run with verbose output
ctest --test-dir build-debug -V
```

---

## Verification

1. `cmake --preset test` configures without errors
2. `cmake --build --preset test` compiles dc:: libraries, test executables, and the
   main application
3. `dc_unit_tests` links without JUCE symbols (verify with `nm` or `objdump`)
4. `ctest --test-dir build-debug` exits 0 (initially 0 tests, then grows)
5. Adding `#include <JuceHeader.h>` to any dc:: source file causes a compile error
   in the `dc_*` library target
