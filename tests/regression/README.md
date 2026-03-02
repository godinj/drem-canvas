# Regression Tests

Every bug found during development becomes a permanent test case. Regression tests
are never deleted.

## File Naming

```
issue_NNN_short_description.cpp
```

Where `NNN` is the issue number (or a sequential counter if no issue tracker is used).

## Template

```cpp
// tests/regression/issue_NNN_short_description.cpp
//
// Bug: [One-line description of the bug]
// Cause: [Root cause]
// Fix: [What was changed to fix it]

#include <catch2/catch_test_macros.hpp>
#include "dc/model/PropertyTree.h"  // or relevant header

TEST_CASE("Regression #NNN: short description", "[regression]")
{
    // Setup: reproduce the conditions that triggered the bug
    // ...

    // Action: perform the operation that was buggy
    // ...

    // Verify: the bug no longer occurs
    REQUIRE(/* correct behaviour */);
}
```

## Workflow

1. Bug is discovered (during agent session, testing, or user report)
2. Write a test that reproduces the bug (test should fail without the fix)
3. Fix the bug
4. Verify test passes
5. Commit test alongside fix -- test is permanent

## CMake Integration

Regression tests are automatically picked up via glob in `tests/CMakeLists.txt`:

```cmake
file(GLOB REGRESSION_TESTS tests/regression/*.cpp)
target_sources(dc_unit_tests PRIVATE ${REGRESSION_TESTS})
```

## Tags

All regression tests must use the `[regression]` Catch2 tag. To run only regression
tests:

```bash
./build-debug/dc_unit_tests "[regression]"
```
