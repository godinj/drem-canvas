# Agent: Verification Scripts & Architecture Guards

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the test harness: create architecture enforcement scripts,
add compile-time `#error` guards to dc:: headers, and configure `.clang-tidy`.

## Context

Read these specs before starting:

- `docs/test-harness/03-architecture-guards.md` (full design — all three enforcement layers)
- `docs/test-harness/00-prd.md` (architecture overview)
- `docs/sans-juce/08-migration-guide.md` (boundary files and JUCE API markers)
- `src/dc/foundation/types.h` (existing foundation header)
- `src/dc/model/PropertyTree.h` (existing model header)
- `src/dc/midi/MidiMessage.h` (existing MIDI header)
- `src/dc/audio/AudioBlock.h` (existing audio header)

## Prerequisites

Phase 1 (CMake infrastructure) must be completed. The dc:: library targets
(`dc_foundation`, `dc_model`, `dc_midi`, `dc_audio`) must exist in `CMakeLists.txt`.

## Deliverables

### 1. scripts/check_architecture.sh

A POSIX shell script with the following checks (see spec for exact grep patterns):

1. **JUCE-free dc:: libraries** — No `juce::`, `#include.*Juce`, or `JuceHeader`
   in any file under `src/dc/`
2. **JUCE boundary enforcement** — `juce::String`, `juce::File`, `juce::Array`,
   `juce::Colour` only in documented boundary files (exclude `src/gui/`,
   `src/plugins/`, `src/Main.cpp`, `*Processor.h`, `// JUCE API boundary` comments)
3. **ColourBridge enforcement** — No direct `juce::Colour(` in `src/gui/` or
   `src/ui/` (must use `dc::bridge::toJuce()`)
4. **Real-time safety** — No heap allocation, blocking, or I/O in
   `src/engine/*Processor.cpp` (allow `// RT-safe:` and `// not on audio thread`
   comment overrides)
5. **dc:: header self-containment** — Each dc:: `.h` file compiles independently

The script must:
- Exit 0 on success, non-zero on any violation
- Print file:line details for each violation
- Be runnable with `bash scripts/check_architecture.sh`
- Make the script executable (`chmod +x`)

### 2. Compile-Time #error Guards

Add `#error` directives to one key header per dc:: library. Place them at the top
of the file, before any `#include` or namespace declarations:

#### src/dc/foundation/types.h

```cpp
#ifdef JUCE_CORE_H_INCLUDED
  #error "dc::foundation must not depend on JUCE — Phase 0 boundary violation"
#endif
```

#### src/dc/model/PropertyTree.h

```cpp
#ifdef JUCE_DATA_STRUCTURES_H_INCLUDED
  #error "dc::model must not depend on JUCE data structures — Phase 1 boundary violation"
#endif
```

#### src/dc/midi/MidiMessage.h

```cpp
#ifdef JUCE_AUDIO_BASICS_H_INCLUDED
  #error "dc::midi must not depend on JUCE audio basics — Phase 2 boundary violation"
#endif
```

#### src/dc/audio/AudioBlock.h

```cpp
#ifdef JUCE_AUDIO_BASICS_H_INCLUDED
  #error "dc::audio must not depend on JUCE audio basics — Phase 2 boundary violation"
#endif
```

### 3. .clang-tidy

Create `.clang-tidy` at the project root:

```yaml
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

### 4. Static Assertions

Add lock-free guarantees to engine code. If `src/engine/TransportController.h` uses
`std::atomic<int64_t>`, add:

```cpp
static_assert(std::atomic<int64_t>::is_always_lock_free,
    "Transport position must be lock-free on this platform");
```

## Important

- The `#error` guards must NOT break the existing application build. They only fire
  if a dc:: source file is compiled in a translation unit that also includes JUCE
  headers. Since the dc:: library targets do not link JUCE, this cannot happen in
  normal builds. The guards catch accidental includes in dc:: files themselves.
- The `check_architecture.sh` script must work on both Linux and macOS (use POSIX
  grep, not GNU-only flags).
- Test that the script passes on the current codebase before committing.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Script verification: `scripts/check_architecture.sh` exits 0
- Build verification: `cmake --build --preset release` still builds the full app
