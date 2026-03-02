# 03 — Architecture Guards & Boundary Enforcement

> Three layers of enforcement — script, compile-time, and link-time — to prevent
> JUCE dependencies from leaking back into dc:: libraries and to enforce real-time
> safety on the audio thread.

**Phase**: 2 (Architecture Guards)
**Dependencies**: Phase 1 (CMake infrastructure — library targets must exist)
**Related**: [00-prd.md](00-prd.md), [01-cmake-infrastructure.md](01-cmake-infrastructure.md),
[04-agent-management.md](04-agent-management.md),
[../sans-juce/08-migration-guide.md](../sans-juce/08-migration-guide.md)

---

## Overview

The sans-JUCE migration established boundaries: `dc::foundation` has no external
dependencies, `dc::model` depends only on foundation, and `dc::midi`/`dc::audio`
depend on foundation plus system libraries (never JUCE). These boundaries must be
enforced permanently — otherwise AI agents and human developers will inadvertently
re-introduce JUCE dependencies.

Three layers of enforcement catch violations at different points:

1. **Script-based** (`scripts/check_architecture.sh`) — fast grep checks, runnable
   in hooks and CI
2. **Compile-time** (`#error` guards in dc:: headers) — instant feedback on include
3. **Link-time** (CMake target separation) — strongest guarantee; dc:: targets do not
   link JUCE, so JUCE symbols cannot resolve

---

## Layer 1 — Script-Based Enforcement

### scripts/check_architecture.sh

A POSIX shell script that greps dc:: source files for forbidden patterns. Exits 0 on
success, non-zero on violation with file:line details.

#### Checks

**1. JUCE-free dc:: libraries**

No dc:: source file may contain JUCE includes or JUCE type references:

```bash
# Check dc:: headers and sources for JUCE contamination
JUCE_PATTERNS='juce::|#include.*Juce|#include.*juce_|JuceHeader'
DC_DIRS="src/dc/foundation src/dc/model src/dc/midi src/dc/audio"

for dir in $DC_DIRS; do
    if grep -rn -E "$JUCE_PATTERNS" "$dir"; then
        echo "FAIL: JUCE reference found in $dir"
        exit 1
    fi
done
```

**2. JUCE boundary enforcement outside dc::**

Outside dc:: libraries, `juce::String`, `juce::File`, `juce::Array`, and
`juce::Colour` should only appear in documented boundary files:

```bash
BOUNDARY_TYPES='juce::String|juce::File|juce::Array|juce::Colour[^B]'
BOUNDARY_EXCLUDES='src/gui/|src/plugins/|src/Main.cpp|Processor\.h'

grep -rn -E "$BOUNDARY_TYPES" src/ \
    --include='*.h' --include='*.cpp' \
    | grep -v -E "$BOUNDARY_EXCLUDES" \
    | grep -v '// JUCE API boundary'
```

**3. ColourBridge enforcement**

GUI files must not construct `juce::Colour` directly — they must use
`dc::bridge::toJuce()`:

```bash
# Direct juce::Colour construction in GUI code is forbidden
grep -rn 'juce::Colour(' src/gui/ src/ui/ \
    --include='*.h' --include='*.cpp' \
    | grep -v 'ColourBridge'
```

**4. Real-time safety (engine files)**

Engine `*Processor.cpp` files must not contain heap allocation, blocking, or I/O:

```bash
RT_FORBIDDEN='(^|[^a-zA-Z_])(new |delete |malloc|free|realloc|calloc)\b'
RT_FORBIDDEN+='|std::mutex|lock_guard|unique_lock|condition_variable'
RT_FORBIDDEN+='|std::cout|std::cerr|printf|fprintf|fopen|fwrite'
RT_FORBIDDEN+='|std::thread|pthread_create'

grep -rn -E "$RT_FORBIDDEN" src/engine/*Processor.cpp \
    | grep -v '// RT-safe:' \
    | grep -v '// not on audio thread'
```

**5. dc:: header self-containment**

Each dc:: header must compile independently (no missing includes):

```bash
for header in $(find src/dc -name '*.h'); do
    echo "#include \"$header\"" | \
        c++ -std=c++17 -fsyntax-only -I src/ -I build/generated/ -x c++ - \
        || echo "FAIL: $header is not self-contained"
done
```

#### Usage

```bash
# Run all checks
scripts/check_architecture.sh

# In CI
- name: Architecture check
  run: scripts/check_architecture.sh
```

---

## Layer 2 — Compile-Time Guards

Place `#error` directives at the top of key dc:: headers. These fire immediately if
a translation unit includes both a dc:: header and a JUCE header — catching violations
at compile time rather than waiting for a script run.

### Foundation Guard

```cpp
// src/dc/foundation/types.h (or a dedicated no_juce.h)
#ifdef JUCE_CORE_H_INCLUDED
  #error "dc::foundation must not depend on JUCE — Phase 0 boundary violation. " \
         "See docs/sans-juce/04-foundation-types.md"
#endif
```

### Model Guard

```cpp
// src/dc/model/PropertyTree.h
#ifdef JUCE_DATA_STRUCTURES_H_INCLUDED
  #error "dc::model must not depend on JUCE data structures — Phase 1 boundary violation. " \
         "See docs/sans-juce/01-observable-model.md"
#endif
```

### MIDI Guard

```cpp
// src/dc/midi/MidiMessage.h
#ifdef JUCE_AUDIO_BASICS_H_INCLUDED
  #error "dc::midi must not depend on JUCE audio basics — Phase 2 boundary violation. " \
         "See docs/sans-juce/06-midi-subsystem.md"
#endif
```

### Audio Guard

```cpp
// src/dc/audio/AudioBlock.h
#ifdef JUCE_AUDIO_BASICS_H_INCLUDED
  #error "dc::audio must not depend on JUCE audio basics — Phase 2 boundary violation. " \
         "See docs/sans-juce/05-audio-io.md"
#endif
```

### Where to Place Guards

Place the `#error` directive at the top of one representative header per library
(before any includes or declarations). This header must be transitively included by
all other headers in the library. If no single header covers all, place guards in
multiple headers.

---

## Layer 3 — Link-Time Enforcement (CMake)

This is the strongest layer. The dc:: library targets defined in Phase 1 do not list
JUCE in `target_link_libraries()`. Therefore:

- JUCE include directories are not available → `#include <JuceHeader.h>` fails at
  compile time
- JUCE symbols are not linked → any `juce::` function call fails at link time

This requires no maintenance — it is structural. As long as the CMake targets remain
separate, the boundary holds automatically.

```cmake
# dc_foundation links NO external libraries (except Threads)
target_link_libraries(dc_foundation PUBLIC Threads::Threads)

# dc_model links only dc_foundation
target_link_libraries(dc_model PUBLIC dc_foundation)

# dc_midi links only dc_foundation
target_link_libraries(dc_midi PUBLIC dc_foundation)

# dc_audio links dc_foundation + system audio libs (NOT JUCE)
target_link_libraries(dc_audio PUBLIC dc_foundation ${SNDFILE_LIB})
```

Any attempt to use JUCE APIs in these targets will fail during build.

---

## Real-Time Safety Enforcement

### Static Analysis

The `scripts/check_architecture.sh` script catches textual patterns (see Layer 1,
check 4). Additionally, the `.clang-tidy` config flags unsafe patterns:

```yaml
Checks: >
  -*, bugprone-*, cppcoreguidelines-*, modernize-*, performance-*,
  readability-*, concurrency-mt-unsafe,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers

WarningsAsErrors: >
  bugprone-use-after-move,
  cppcoreguidelines-no-malloc,
  concurrency-mt-unsafe
```

### Compile-Time Static Assertions

Place in engine code to verify lock-free guarantees:

```cpp
// src/engine/TransportController.h
static_assert(std::atomic<int64_t>::is_always_lock_free,
    "Transport position must be lock-free on this platform");

static_assert(sizeof(TransportState) <= 64,
    "TransportState must fit in a cache line for atomic operations");
```

### RealtimeSanitizer (RTSan)

Available in LLVM 20.0.0+. Annotate audio callbacks:

```cpp
[[clang::nonblocking]]
void processBlock(dc::AudioBlock& block, dc::MidiBuffer& midi)
{
    // Any heap allocation, mutex lock, or I/O in this scope
    // triggers a runtime error when compiled with -fsanitize=realtime
}
```

Compile with:

```bash
cmake --preset test -DCMAKE_CXX_FLAGS="-fsanitize=realtime"
```

Suppress false positives:

```cpp
__rtsan::ScopedDisabler disabler; // Temporarily allow allocations
```

RTSan is opt-in and requires LLVM 20+. Use it in CI when the toolchain supports it.

### Qiti (Audio Test Framework)

Embed heap allocation trackers directly in test cases:

```cpp
TEST_CASE("Audio callback is RT-safe")
{
    qiti::HeapTracker tracker;
    tracker.beginTracking();
    audioEngine.processBlock(testBuffer, 512);
    REQUIRE(tracker.getAllocationCount() == 0);
}
```

This verifies at runtime that no allocations occur during audio processing, without
requiring `-fsanitize=realtime`.

---

## .clang-tidy Configuration

Place at project root:

```yaml
# .clang-tidy
Checks: >
  -*,
  bugprone-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  concurrency-mt-unsafe,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers,
  -cppcoreguidelines-avoid-magic-numbers,
  -readability-identifier-length,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic

WarningsAsErrors: >
  bugprone-use-after-move,
  cppcoreguidelines-no-malloc,
  concurrency-mt-unsafe

HeaderFilterRegex: 'src/dc/.*'

CheckOptions:
  - key: readability-function-cognitive-complexity.Threshold
    value: 25
```

### Running clang-tidy

```bash
# Single file
clang-tidy src/dc/foundation/base64.cpp -- -std=c++17 -I src/ -I build/generated/

# All dc:: files (using compile_commands.json)
cmake --preset debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
run-clang-tidy -p build-debug/ 'src/dc/.*'

# In verify.sh (changed files only)
git diff --name-only HEAD | grep 'src/dc/' | while read f; do
    clang-tidy "$f" -p build-debug/ || exit 1
done
```

---

## Include-What-You-Use (IWYU)

Prevents the "transitive include" problem where code compiles only because another
header happens to include a needed dependency.

```bash
# Install
sudo apt install iwyu

# Run
cmake --preset debug -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=iwyu
cmake --build --preset debug 2>&1 | tee iwyu.log
```

IWYU is advisory — it suggests changes but does not fail the build. Run periodically,
not on every commit.

---

## Verification

1. `scripts/check_architecture.sh` exits 0 on clean codebase
2. Intentionally add `#include <JuceHeader.h>` to `src/dc/foundation/types.h`:
   - Script check catches it
   - `#error` guard fires at compile time
   - CMake build fails (JUCE headers not on include path)
3. `.clang-tidy` runs without errors on `src/dc/` files
4. `static_assert` on `atomic<int64_t>` passes on x86_64 and ARM64
5. Real-time safety grep finds no violations in clean `*Processor.cpp` files
