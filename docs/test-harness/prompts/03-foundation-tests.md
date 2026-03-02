# Agent: Foundation Tests

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 3 of the test harness: write unit tests for all dc::foundation
types that replaced JUCE foundation equivalents.

## Context

Read these specs before starting:

- `docs/test-harness/02-migration-tests.md` (Phase 0 — Foundation Tests section)
- `docs/test-harness/00-prd.md` (test directory layout)
- `src/dc/foundation/types.h` (Colour, roundToInt, pi, approxEqual, randomFloat, randomInt)
- `src/dc/foundation/string_utils.h` (trim, replace, contains, startsWith, afterFirst, shellQuote, format)
- `src/dc/foundation/base64.h` + `base64.cpp` (base64Encode, base64Decode)
- `src/dc/foundation/spsc_queue.h` (SpscQueue<T>)
- `src/dc/foundation/message_queue.h` + `message_queue.cpp` (MessageQueue)
- `src/dc/foundation/listener_list.h` (ListenerList<T>)
- `src/dc/foundation/worker_thread.h` + `worker_thread.cpp` (WorkerThread)

Read each source file to understand the exact API before writing tests.

## Prerequisites

Phase 1 (CMake infrastructure) and Phase 2 (verification scripts) must be completed.
The `dc_foundation` library target and `dc_unit_tests` executable must exist.

## Deliverables

Create 8 test files in `tests/unit/foundation/`. Add each file to
`target_sources(dc_unit_tests ...)` in `tests/CMakeLists.txt`.

### 1. tests/unit/foundation/test_colour.cpp

Test `dc::Colour`:
- `fromRGB()`, `fromFloat()`, `fromHSV()` construction
- `toHexString()` / `fromHexString()` round-trip
- `brighter()`, `darker()` at extremes (white stays white, black stays black)
- `interpolatedWith()` at t=0, t=0.5, t=1
- `withAlpha()` preserves RGB
- `Colours::` preset constants spot-check

Use `SECTION` blocks to group related tests under one `TEST_CASE`.

### 2. tests/unit/foundation/test_string_utils.cpp

Test all functions in `dc::string_utils`:
- `trim()`: empty, normal, all-whitespace
- `replace()`: all occurrences, empty from (no-op), overlapping matches
- `contains()`: present/absent substring
- `startsWith()`: match/mismatch, empty prefix
- `afterFirst()`: delimiter at end, delimiter absent
- `shellQuote()`: spaces, embedded `'`, empty, control chars
- `format()`: valid format, no args

### 3. tests/unit/foundation/test_base64.cpp

Test `dc::base64Encode()` / `dc::base64Decode()`:
- Round-trip for sizes 0, 1, 2, 3, 4, 5, 100, 1000
- Empty input for both encode and decode
- Whitespace tolerance in decode
- Padding variations

### 4. tests/unit/foundation/test_random.cpp

Test `dc::randomFloat()` and `dc::randomInt()`:
- Range checks (1000 iterations)
- `randomInt(5, 5)` always returns 5
- Thread safety (concurrent calls from multiple threads)

### 5. tests/unit/foundation/test_spsc_queue.cpp

Test `dc::SpscQueue<T>`:
- Push/pop FIFO ordering
- Full queue push returns false
- Empty queue pop returns false
- Capacity rounds to power of 2
- Wrap-around after capacity items
- Multi-threaded stress: producer pushes 100k sequential ints, consumer verifies order
- Move-only types (`std::unique_ptr<int>`)

### 6. tests/unit/foundation/test_message_queue.cpp

Test `dc::MessageQueue`:
- Post then processAll fires callback
- FIFO ordering of multiple callbacks
- `pending()` count
- Callback that posts new callback (deferred to next processAll)
- Multi-thread post safety

### 7. tests/unit/foundation/test_listener_list.cpp

Test `dc::ListenerList<T>`:
- Add listener, call, listener receives callback
- Remove listener, no longer called
- Duplicate add silently ignored
- Remove during callback is safe
- Add during callback — new listener not called in current iteration
- Call on empty list is no-op

### 8. tests/unit/foundation/test_worker_thread.cpp

Test `dc::WorkerThread`:
- Submit task, verify execution on background thread
- Task ordering (tasks execute in submission order)
- `stop()` waits for current task
- `stop()` multiple times (idempotent)
- Submit after `stop()` (silently ignored)
- `isRunning()` state transitions

## Important

- Read the actual source code in `src/dc/foundation/` before writing tests.
  The spec provides the expected invariants, but the actual API may have minor
  differences — match the real code.
- Include only dc:: headers in test files. Never include `<JuceHeader.h>`.
- Use `#include <catch2/catch_test_macros.hpp>` as the main Catch2 header.
- Use descriptive string test names: `TEST_CASE("Colour fromRGB creates opaque colour")`
- Use `SECTION` blocks for shared setup within a TEST_CASE.
- For multi-threaded tests, use `<thread>` and `<atomic>` from the standard library.
- For floating-point comparisons, use `Catch::Approx` or `Catch::Matchers::WithinAbs`.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `tests/CMakeLists.txt` `target_sources(dc_unit_tests ...)`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure`
