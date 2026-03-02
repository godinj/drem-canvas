# Agent: CMake Test Infrastructure

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 1 of the test harness: extract dc:: sources into separate CMake
library targets, integrate Catch2 v3, create the `tests/` directory structure, and
add CMake presets for testing and coverage.

## Context

Read these specs before starting:

- `docs/test-harness/01-cmake-infrastructure.md` (full design)
- `docs/test-harness/00-prd.md` (architecture overview, test directory layout)
- `CMakeLists.txt` (current monolithic build — study existing targets and dependencies)
- `CMakePresets.json` (current presets — you will add `test` and `coverage`)

## Deliverables

### 1. Separate dc:: Library Targets in CMakeLists.txt

Extract four static library targets from the monolithic `DremCanvas` executable.
Each dc:: source file must move from `target_sources(DremCanvas ...)` to the
appropriate library target.

#### dc_foundation

```cmake
add_library(dc_foundation STATIC
    src/dc/foundation/base64.cpp
    src/dc/foundation/message_queue.cpp
    src/dc/foundation/worker_thread.cpp
)
target_include_directories(dc_foundation PUBLIC src/ ${CMAKE_BINARY_DIR}/generated/)
target_compile_features(dc_foundation PUBLIC cxx_std_17)
find_package(Threads REQUIRED)
target_link_libraries(dc_foundation PUBLIC Threads::Threads)
```

No JUCE. No Skia. No third-party libraries.

#### dc_model

```cmake
add_library(dc_model STATIC
    src/dc/model/Variant.cpp
    src/dc/model/PropertyId.cpp
    src/dc/model/PropertyTree.cpp
    src/dc/model/UndoManager.cpp
)
target_link_libraries(dc_model PUBLIC dc_foundation)
```

#### dc_midi

```cmake
add_library(dc_midi STATIC
    src/dc/midi/MidiMessage.cpp
    src/dc/midi/MidiBuffer.cpp
    src/dc/midi/MidiSequence.cpp
)
target_link_libraries(dc_midi PUBLIC dc_foundation)
```

Exclude `MidiDeviceManager.cpp` (depends on RtMidi). It stays in the main target.

#### dc_audio

```cmake
add_library(dc_audio STATIC
    src/dc/audio/AudioFileReader.cpp
    src/dc/audio/AudioFileWriter.cpp
    src/dc/audio/DiskStreamer.cpp
    src/dc/audio/ThreadedRecorder.cpp
)
target_link_libraries(dc_audio PUBLIC dc_foundation Threads::Threads)
```

Add libsndfile linkage (platform-specific — see the spec). Exclude
`PortAudioDeviceManager.cpp` (depends on PortAudio). It stays in the main target.

#### Update DremCanvas Target

Remove the dc:: `.cpp` files from `target_sources(DremCanvas ...)` and instead
link the library targets:

```cmake
target_link_libraries(DremCanvas PRIVATE dc_foundation dc_model dc_midi dc_audio ...)
```

### 2. Catch2 v3 Integration

Add `BUILD_TESTING` option and Catch2 FetchContent (v3.8.1). See the spec for the
exact FetchContent block, including the critical `CMAKE_MODULE_PATH` line for
`catch_discover_tests()`.

Also fetch Trompeloeil (v49) for mocking.

Wrap everything in `if(BUILD_TESTING) ... endif()`.

### 3. Test Directory Structure

Create the following directories and placeholder files:

```
tests/
├── CMakeLists.txt
├── unit/
│   ├── foundation/
│   ├── model/
│   ├── midi/
│   ├── audio/
│   ├── model_layer/
│   ├── vim/
│   └── engine/
├── integration/
│   └── main.cpp       # Custom main with ScopedJuceInitialiser_GUI
├── regression/
│   └── .gitkeep
├── fuzz/
├── golden/
└── fixtures/
```

Create `tests/CMakeLists.txt` with the `dc_unit_tests` executable target (initially
empty source list — test files will be added in later phases). Link it against
`dc_foundation dc_model dc_midi dc_audio Catch2::Catch2WithMain trompeloeil::trompeloeil`.

Use `catch_discover_tests(dc_unit_tests)` for CTest auto-registration.

### 4. CMake Presets

Add to `CMakePresets.json`:

- `test` preset: inherits debug, sets `BUILD_TESTING=ON`, binary dir `build-debug`
- `coverage` preset: inherits debug, sets `BUILD_TESTING=ON` plus
  `-fprofile-instr-generate -fcoverage-mapping`, binary dir `build-coverage`
- Corresponding build presets for both

## Important

- The dc:: library targets must NOT link JUCE, Skia, or any GUI framework.
  This is the primary architectural guard.
- `AudioBlock.h` is header-only — it does not need to be in `target_sources()`.
- The `configure_file()` for `config.h.in` must run before `dc_foundation` compiles
  (it generates `dc/foundation/config.h`).
- After restructuring, `cmake --build --preset release` must still build the full
  application successfully. Do not break the existing build.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release` (existing app) and
  `cmake --preset test && cmake --build --preset test` (new test targets)
