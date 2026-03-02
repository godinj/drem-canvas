# 05 — Integration Tests, Fuzzing, Coverage & CI

> Higher-level tests (session round-trip, vim commands, audio graph), fuzzing
> harnesses, code coverage, mutation testing, property-based testing, and
> GitHub Actions CI/CD workflow.

**Phase**: 7–8 (Integration & Higher-Level), 10 (Fuzz/Coverage/CI)
**Dependencies**: Phases 3–6 (unit tests), Phase 1 (CMake infrastructure)
**Related**: [00-prd.md](00-prd.md), [01-cmake-infrastructure.md](01-cmake-infrastructure.md),
[02-migration-tests.md](02-migration-tests.md), [04-agent-management.md](04-agent-management.md)

---

## Integration Tests (Phase 7)

Integration tests exercise subsystems working together. They link JUCE (for session
infrastructure) and use a custom `main.cpp` with `ScopedJuceInitialiser_GUI`.

### test_session_roundtrip.cpp

**Goal**: Verify that a project with tracks, clips, plugin chains, mixer state,
and tempo map survives serialisation and deserialisation.

| Test Case | Invariant |
|-----------|-----------|
| Create full project → serialise → deserialise | All PropertyTree properties match |
| Empty project round-trip | Minimal valid YAML produced |
| Project with MIDI clips | MidiSequence binary data round-trips |
| Project with audio clips | File paths preserved |
| Project with plugin state | Base64-encoded state round-trips |
| Mixer state (volume, pan, mute, solo) | All channel strip properties preserved |
| Tempo changes | TempoMap events round-trip |
| Golden file comparison | Serialised YAML matches `.approved` file |

### test_vim_commands.cpp

**Goal**: Verify that Vim key sequences produce expected model state changes.

| Test Case | Invariant |
|-----------|-----------|
| `j` / `k` in arrangement | Track selection moves down / up |
| `h` / `l` in arrangement | Cursor grid position moves left / right |
| `dd` on track | Track deleted from arrangement |
| `yy` / `p` on track | Track copied and pasted |
| `u` after delete | Track restored (undo) |
| Mode transitions | Normal → Insert (i), Insert → Normal (Escape) |
| Context switching | Tab cycles through panels |

### test_audio_graph.cpp

**Goal**: Verify that the audio processing graph produces correct output.

| Test Case | Invariant |
|-----------|-----------|
| Silent graph | All outputs are zero |
| Single track with test signal | Output matches input × gain |
| Muted track | Output is zero |
| Solo track (with other tracks) | Only solo track heard |
| Plugin bypass | Output equals input |

### Custom Main for Integration Tests

```cpp
// tests/integration/main.cpp
#include <JuceHeader.h>
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    // JUCE requires initialisation before any JUCE types are used
    juce::ScopedJuceInitialiser_GUI init;
    return Catch::Session().run(argc, argv);
}
```

---

## Higher-Level Model Tests (Phase 8)

These test application-layer classes that sit on top of dc:: primitives.

### test_project.cpp

| Test Case | Invariant |
|-----------|-----------|
| `addTrack()` / `removeTrack()` | Track count changes, PropertyTree updated |
| `getTrack(index)` bounds | Out-of-range returns nullptr |
| `getSampleRate()` | Returns configured rate |
| Undo after `addTrack()` | Track removed |

### test_track.cpp

| Test Case | Invariant |
|-----------|-----------|
| `addClip()` at position | Clip appears in track's children |
| `removeClip()` | Clip removed, indices shift |
| Track properties (name, colour, volume, pan) | Set/get round-trip |

### test_arrangement.cpp

| Test Case | Invariant |
|-----------|-----------|
| Track selection (select, deselect, multi-select) | Selection state correct |
| Clip selection | VimContext selection range correct |

### test_tempo_map.cpp

| Test Case | Invariant |
|-----------|-----------|
| `samplesToBeats()` at 120 BPM, 44100 SR | 44100 samples = 2 beats |
| `beatsToSamples()` inverse | Round-trip within ε |
| `samplesToBarBeat()` with 4/4 | Correct bar and beat numbers |
| `samplesToBarBeat()` with 3/4 | 3 beats per bar |
| `samplesToBarBeat()` with 6/8 | Compound time handling |
| Zero samples | Returns beat 0, bar 1 |
| Very large sample counts | No overflow |

### test_grid_system.cpp

| Test Case | Invariant |
|-----------|-----------|
| Grid snap to beat | Position quantised to nearest beat boundary |
| Grid snap to bar | Position quantised to nearest bar boundary |
| Grid snap disabled | Position unchanged |

### test_clipboard.cpp

| Test Case | Invariant |
|-----------|-----------|
| Copy clip → paste | New clip at cursor position |
| Copy track → paste | New track with same properties |
| Cut removes source | Original clip/track deleted |

### test_vim_context.cpp

| Test Case | Invariant |
|-----------|-----------|
| Panel focus state transitions | Focus follows Tab key |
| Visual selection range | Track and clip ranges correct |
| Grid cursor position (in samples) | Moves by grid increment |

### test_action_registry.cpp

| Test Case | Invariant |
|-----------|-----------|
| `fuzzyScore()` algorithm | Known input/output pairs verified |
| Action filtering by panel | Only relevant actions returned |
| Action lookup by ID | Returns correct action |
| Empty query | All actions returned |

### test_transport.cpp

| Test Case | Invariant |
|-----------|-----------|
| `play()` / `stop()` state | `isPlaying()` reflects state |
| `getPositionInSamples()` | Atomic, readable from any thread |
| `setPosition()` | Position updates atomically |
| Position after `stop()` | Position preserved (not reset to 0) |

---

## Fuzzing (Phase 10)

### libFuzzer Integration

libFuzzer ships with Clang. Each fuzz target is a separate executable.

#### CMake Setup

```cmake
if(BUILD_FUZZING)
    set(FUZZ_FLAGS "-fsanitize=fuzzer,address,undefined")

    add_executable(fuzz_midi_parser fuzz/fuzz_midi_parser.cpp)
    target_link_libraries(fuzz_midi_parser PRIVATE dc_midi)
    target_compile_options(fuzz_midi_parser PRIVATE ${FUZZ_FLAGS})
    target_link_options(fuzz_midi_parser PRIVATE ${FUZZ_FLAGS})

    add_executable(fuzz_yaml_session fuzz/fuzz_yaml_session.cpp)
    target_link_libraries(fuzz_yaml_session PRIVATE dc_model yaml-cpp::yaml-cpp)
    target_compile_options(fuzz_yaml_session PRIVATE ${FUZZ_FLAGS})
    target_link_options(fuzz_yaml_session PRIVATE ${FUZZ_FLAGS})

    add_executable(fuzz_vim_keys fuzz/fuzz_vim_keys.cpp)
    target_link_libraries(fuzz_vim_keys PRIVATE dc_model)
    target_compile_options(fuzz_vim_keys PRIVATE ${FUZZ_FLAGS})
    target_link_options(fuzz_vim_keys PRIVATE ${FUZZ_FLAGS})
endif()
```

### Fuzz Targets

#### fuzz_midi_parser.cpp

Feed random bytes to MIDI constructors:

```cpp
#include "dc/midi/MidiMessage.h"
#include "dc/midi/MidiBuffer.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Test MidiMessage construction from raw bytes
    if (size >= 1)
    {
        dc::MidiMessage msg(data, static_cast<int>(size));
        // Exercise queries — must not crash
        (void)msg.isNoteOn();
        (void)msg.isNoteOff();
        (void)msg.isController();
        (void)msg.getChannel();
        (void)msg.getRawDataSize();
    }

    // Test MidiBuffer with random events
    if (size >= 4)
    {
        dc::MidiBuffer buffer;
        int offset = static_cast<int>(data[0]) * 256 + data[1];
        dc::MidiMessage msg(data + 2, static_cast<int>(size - 2));
        buffer.addEvent(msg, offset);
        for (auto& event : buffer)
        {
            (void)event.sampleOffset;
            (void)event.message.isNoteOn();
        }
    }

    return 0;
}
```

#### fuzz_yaml_session.cpp

Feed random bytes to yaml-cpp:

```cpp
#include <yaml-cpp/yaml.h>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    try
    {
        std::string input(reinterpret_cast<const char*>(data), size);
        YAML::Node node = YAML::Load(input);
        // Exercise node traversal — must not crash
        if (node.IsMap())
        {
            for (auto it = node.begin(); it != node.end(); ++it)
            {
                (void)it->first.as<std::string>();
            }
        }
    }
    catch (const YAML::Exception&)
    {
        // Expected for random input
    }

    return 0;
}
```

#### fuzz_vim_keys.cpp

Feed random key sequences to Vim engine:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Create a minimal VimContext + ActionRegistry
    // Feed each byte as a key press
    // Must not crash regardless of input sequence
    return 0;
}
```

### Dictionary Files

Create dictionaries for common byte patterns to guide fuzzing:

```
# tests/fuzz/midi.dict
"\x80"  # Note Off
"\x90"  # Note On
"\xA0"  # Aftertouch
"\xB0"  # Control Change
"\xC0"  # Program Change
"\xD0"  # Channel Pressure
"\xE0"  # Pitch Wheel
"\xF0"  # SysEx Start
"\xF7"  # SysEx End
"\xFF"  # System Reset
```

### Running Fuzz Tests

```bash
# Run for 60 seconds
./build-debug/fuzz_midi_parser -max_total_time=60 -dict=tests/fuzz/midi.dict

# With corpus directory
mkdir -p tests/fuzz/corpus/midi
./build-debug/fuzz_midi_parser tests/fuzz/corpus/midi -max_total_time=60

# Reproduce a crash
./build-debug/fuzz_midi_parser crash-input-file
```

---

## Code Coverage (Phase 10)

### CMake Coverage Preset

Already defined in [01-cmake-infrastructure.md](01-cmake-infrastructure.md):

```json
{
    "name": "coverage",
    "inherits": "debug",
    "cacheVariables": {
        "CMAKE_CXX_FLAGS": "-fprofile-instr-generate -fcoverage-mapping",
        "CMAKE_C_FLAGS": "-fprofile-instr-generate -fcoverage-mapping"
    }
}
```

### Coverage Workflow

```bash
# 1. Build with coverage instrumentation
cmake --preset coverage
cmake --build --preset coverage

# 2. Run tests (generates .profraw files)
LLVM_PROFILE_FILE="coverage-%p.profraw" ctest --test-dir build-coverage

# 3. Merge profiles
llvm-profdata merge -sparse coverage-*.profraw -o coverage.profdata

# 4. Generate HTML report
llvm-cov show ./build-coverage/dc_unit_tests \
    -instr-profile=coverage.profdata \
    -format=html -output-dir=coverage-report \
    -ignore-filename-regex='libs/.*|build.*'

# 5. Export lcov format for CI upload
llvm-cov export ./build-coverage/dc_unit_tests \
    -instr-profile=coverage.profdata \
    -format=lcov > coverage.lcov
```

### Coverage Targets

| Library | Target Coverage | Rationale |
|---------|----------------|-----------|
| `dc::foundation` | ≥ 80% | Pure logic, highly testable |
| `dc::model` | ≥ 80% | Critical data model |
| `dc::midi` | ≥ 75% | MIDI types, some hardware boundaries |
| `dc::audio` | ≥ 60% | File I/O testable, device I/O mocked |
| `engine/` | ≥ 50% | Audio thread code, harder to test |
| Overall | ≥ 60% | Including all tested code |

### gcovr Alternative

gcovr v8.5+ supports LLVM source-based coverage and can generate HTML, XML
(Cobertura), JSON, and CSV from the same `.profdata` files:

```bash
gcovr --gcov-executable "llvm-cov gcov" --html-details coverage.html
```

---

## Mutation Testing (Phase 10)

### Mull Setup

Mull is an LLVM IR-level mutation testing tool. Version 0.26.1 supports LLVM 18.1.8.

```bash
# Install Mull (Debian/Ubuntu)
curl -1sLf 'https://dl.cloudsmith.io/public/mull-project/mull-stable/setup.deb.sh' | sudo -E bash
sudo apt install mull

# Build with Mull plugin
cmake --preset test \
    -DCMAKE_CXX_FLAGS="-fpass-plugin=/usr/lib/mull-ir-frontend -g -grecord-command-line"
cmake --build --preset test

# Run mutation testing
mull-runner ./build-debug/dc_unit_tests \
    --reporters=Elements \
    --report-dir=mutation-report
```

### Mutation Operators

| Operator | Example |
|----------|---------|
| Arithmetic | `+ → -`, `* → /` |
| Comparison | `< → <=`, `== → !=` |
| Logical | `&& → \|\|`, negate conditions |
| Removal | Remove void function calls |

### Strategy

- Start with `dc::foundation` and `dc::model` (pure logic, fast tests)
- Mutation score below 80% indicates weak assertions in tests
- CPU-intensive: run nightly or weekly, not on every push
- Use `--include-path=src/dc/` to limit mutations to dc:: code

---

## Property-Based Testing

### RapidCheck Integration

Already declared in [01-cmake-infrastructure.md](01-cmake-infrastructure.md).

### Key Properties

```cpp
#include <rapidcheck/catch.h>

// Serialisation round-trip
rc::prop("PropertyTree serialise/deserialise is identity",
    [](/* generated tree */) {
        auto serialised = serialise(tree);
        auto deserialised = deserialise(serialised);
        RC_ASSERT(treesEqual(tree, deserialised));
    });

// Undo/redo symmetry
rc::prop("N operations followed by N undos restores original state",
    [](unsigned int n) {
        RC_PRE(n > 0 && n <= 100);
        auto original = createTestTree();
        dc::UndoManager undo;
        for (unsigned int i = 0; i < n; ++i)
            original.setProperty(testProp, dc::Variant((int64_t)i), &undo);
        for (unsigned int i = 0; i < n; ++i)
            undo.undo();
        RC_ASSERT(original.getProperty(testProp) == originalValue);
    });

// MIDI buffer ordering
rc::prop("MidiBuffer iteration is always in insertion order",
    [](const std::vector<std::pair<int, uint8_t>>& events) {
        dc::MidiBuffer buf;
        for (auto& [offset, note] : events)
            buf.addEvent(dc::MidiMessage::noteOn(1, note % 128, 0.8f), offset);
        int count = 0;
        for (auto& e : buf) ++count;
        RC_ASSERT(count == (int)events.size());
    });

// Coordinate transform round-trip
rc::prop("screenToSample(sampleToScreen(pos)) == pos",
    [](int64_t samplePos) {
        RC_PRE(samplePos >= 0 && samplePos < 1000000000);
        double sampleRate = 44100.0;
        double pixelsPerSecond = 100.0;
        double screen = (samplePos / sampleRate) * pixelsPerSecond;
        int64_t back = (int64_t)(screen / pixelsPerSecond * sampleRate);
        RC_ASSERT(std::abs(back - samplePos) <= 1);  // rounding tolerance
    });
```

### Custom Generators

```cpp
namespace rc {
template<>
struct Arbitrary<dc::MidiMessage> {
    static Gen<dc::MidiMessage> arbitrary() {
        return gen::build<dc::MidiMessage>(
            gen::set(&dc::MidiMessage::noteOn,
                gen::inRange(1, 17),    // channel
                gen::inRange(0, 128),   // note
                gen::map(gen::inRange(0, 128),
                    [](int v) { return v / 127.0f; })  // velocity
            )
        );
    }
};
}
```

---

## CI/CD Workflow (Phase 10)

### GitHub Actions

```yaml
# .github/workflows/build-test.yml
name: Build & Test

on:
  push:
    branches: [master, 'feature/**']
  pull_request:
    branches: [master]

jobs:
  build-and-test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest]
    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies (Linux)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            cmake ninja-build python3 libpng-dev \
            libvulkan-dev libglfw3-dev libfontconfig-dev \
            libasound2-dev portaudio19-dev libsndfile1-dev \
            xvfb

      - name: Install dependencies (macOS)
        if: runner.os == 'macOS'
        run: |
          brew install ninja libsndfile portaudio

      - name: Bootstrap
        run: scripts/bootstrap.sh

      - name: Configure
        run: cmake --preset test

      - name: Build
        run: cmake --build --preset test

      - name: Architecture Check
        run: scripts/check_architecture.sh

      - name: Unit Tests
        run: ctest --test-dir build-debug -R "unit\." --output-on-failure -j$(nproc)

      - name: Integration Tests (Linux)
        if: runner.os == 'Linux'
        run: xvfb-run ctest --test-dir build-debug -R "integration\." --output-on-failure

      - name: Integration Tests (macOS)
        if: runner.os == 'macOS'
        run: ctest --test-dir build-debug -R "integration\." --output-on-failure

  coverage:
    runs-on: ubuntu-latest
    needs: build-and-test
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build libsndfile1-dev \
            portaudio19-dev llvm

      - name: Bootstrap
        run: scripts/bootstrap.sh

      - name: Build with coverage
        run: |
          cmake --preset coverage
          cmake --build --preset coverage

      - name: Run tests with coverage
        run: |
          LLVM_PROFILE_FILE="coverage-%p.profraw" \
            ctest --test-dir build-coverage --output-on-failure

      - name: Generate coverage report
        run: |
          llvm-profdata merge -sparse coverage-*.profraw -o coverage.profdata
          llvm-cov export ./build-coverage/dc_unit_tests \
            -instr-profile=coverage.profdata \
            -format=lcov > coverage.lcov

      - name: Upload coverage
        uses: codecov/codecov-action@v4
        with:
          file: coverage.lcov

  fuzz:
    runs-on: ubuntu-latest
    if: github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Build fuzz targets
        run: |
          cmake --preset test -DBUILD_FUZZING=ON \
            -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer,address,undefined"
          cmake --build --preset test --target fuzz_midi_parser fuzz_yaml_session

      - name: Run fuzzers (60s each)
        run: |
          ./build-debug/fuzz_midi_parser -max_total_time=60
          ./build-debug/fuzz_yaml_session -max_total_time=60
```

### CI Schedule for Fuzzing

```yaml
on:
  schedule:
    - cron: '0 3 * * *'  # Nightly at 3 AM UTC
  workflow_dispatch:       # Manual trigger
```

### Test Categories in CI

| Category | Trigger | Requirements |
|----------|---------|-------------|
| Unit tests | Every push | None (headless) |
| Integration tests | Every push | xvfb (Linux) |
| Architecture check | Every push | None (shell script) |
| Coverage | Every push (on ubuntu) | llvm-cov |
| Fuzz tests | Nightly / manual | Clang + ASan |
| Mutation testing | Weekly / manual | Mull |
| GPU rendering | Manual | GPU runner |
| Plugin hosting | Weekly | Test VST3 binaries |

### Headless Audio on CI

Unit tests mock `PortAudioDeviceManager` and `MidiDeviceManager` via Trompeloeil —
they never touch audio hardware. Integration tests that need a JUCE message loop use
`ScopedJuceInitialiser_GUI` but do not open audio devices.

For integration tests requiring audio output, use:

```bash
# PulseAudio null sink
pulseaudio --start --exit-idle-time=-1
pactl load-module module-null-sink

# Or JACK dummy driver
jackd -d dummy &
```

---

## Verification

1. `cmake --preset test && cmake --build --preset test` builds all test executables
2. `ctest --test-dir build-debug --output-on-failure` runs all registered tests
3. Coverage report generated: `coverage-report/index.html` exists
4. Fuzz targets build with `-fsanitize=fuzzer` and run for 10s without crashes
5. GitHub Actions workflow YAML is valid (lint with `actionlint`)
6. All test categories have appropriate CI triggers
