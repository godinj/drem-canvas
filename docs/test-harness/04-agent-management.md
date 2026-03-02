# 04 — AI Agent Guardrails

> Verification scripts, Claude Code hooks, golden file testing, and regression
> capture workflows that give AI agents automated feedback on correctness.

**Phase**: 9 (Agent Guardrails)
**Dependencies**: Phase 2 (architecture scripts), Phase 1 (test targets)
**Related**: [00-prd.md](00-prd.md), [03-architecture-guards.md](03-architecture-guards.md),
[05-integration-and-advanced.md](05-integration-and-advanced.md)

---

## Overview

AI agents (Claude Code sessions) routinely modify code across the codebase. Without
automated feedback, agents operate blind — they cannot tell whether their changes
break architecture boundaries, introduce real-time safety violations, or cause test
failures.

The guardrail system provides two tiers of verification:

- **Tier 1 (per-edit, ~2s)**: Runs after every file Edit/Write. Catches boundary
  violations and syntax errors instantly.
- **Tier 2 (exit gate, ~30s)**: Runs when the agent finishes. Full build, all tests,
  architecture checks, golden file comparisons.

Additionally, golden file testing (ApprovalTests.cpp) and a regression capture
workflow prevent the same bugs from recurring.

---

## Two-Tier Verification Model

### Tier 1 — Quick Check (PostToolUse Hook)

**Script**: `scripts/quick-check.sh`
**Trigger**: After every `Edit` or `Write` tool use
**Budget**: ≤ 2 seconds

Checks:
1. Architecture boundary grep on changed files only
2. Real-time safety check on engine files (if any changed)
3. Syntax check (compile changed file only, no link)

```bash
#!/usr/bin/env bash
set -euo pipefail

INPUT=$(cat)
TOOL=$(echo "$INPUT" | jq -r '.tool_name')
FILE=$(echo "$INPUT" | jq -r '.tool_input.file_path // .tool_input.command' | head -1)

# Only check on file modifications
case "$TOOL" in
    Edit|Write) ;;
    *) exit 0 ;;
esac

# 1. Architecture boundary check (dc:: files only)
if echo "$FILE" | grep -q 'src/dc/'; then
    JUCE_PATTERNS='juce::|#include.*Juce|#include.*juce_|JuceHeader'
    if grep -n -E "$JUCE_PATTERNS" "$FILE" 2>/dev/null; then
        echo "BOUNDARY VIOLATION: JUCE reference in dc:: file: $FILE"
        exit 1
    fi
fi

# 2. Real-time safety (engine processor files only)
if echo "$FILE" | grep -q 'src/engine/.*Processor\.cpp'; then
    RT_FORBIDDEN='(^|[^a-zA-Z_])(new |delete |malloc|free)\b'
    RT_FORBIDDEN+='|std::mutex|lock_guard|unique_lock'
    if grep -n -E "$RT_FORBIDDEN" "$FILE" 2>/dev/null | grep -v '// RT-safe:'; then
        echo "RT-SAFETY VIOLATION: Forbidden pattern in $FILE"
        exit 1
    fi
fi

# 3. Syntax check (compile only, no link)
if echo "$FILE" | grep -qE '\.(cpp|h)$'; then
    if [ -f build-debug/compile_commands.json ]; then
        # Use compile_commands.json for correct flags
        clang-check "$FILE" -p build-debug/ 2>&1 | head -20 || true
    fi
fi

exit 0
```

### Tier 2 — Full Verification (Stop Hook)

**Script**: `scripts/verify.sh`
**Trigger**: When the agent session ends (Stop hook)
**Budget**: ≤ 30 seconds

Checks:
1. Full build (`cmake --build --preset test`)
2. Full architecture boundary check (`scripts/check_architecture.sh`)
3. Static analysis on changed files (clang-tidy)
4. All unit tests (`ctest --test-dir build-debug`)
5. Golden file comparisons (if any `.approved` files exist)

```bash
#!/usr/bin/env bash
set -euo pipefail

INPUT=$(cat)

# Prevent infinite loop: skip if already inside a Stop hook
if [ "$(echo "$INPUT" | jq -r '.stop_hook_active')" = "true" ]; then
    exit 0
fi

ERRORS=0

# 1. Full build
echo "=== Building ==="
if ! cmake --build --preset test 2>&1 | tail -5; then
    echo "FAIL: Build failed"
    ERRORS=$((ERRORS + 1))
fi

# 2. Architecture check
echo "=== Architecture ==="
if ! scripts/check_architecture.sh; then
    echo "FAIL: Architecture violations found"
    ERRORS=$((ERRORS + 1))
fi

# 3. Static analysis (changed files only)
echo "=== Static Analysis ==="
CHANGED=$(git diff --name-only HEAD 2>/dev/null | grep -E '\.cpp$' | grep 'src/dc/' || true)
for f in $CHANGED; do
    if ! clang-tidy "$f" -p build-debug/ --quiet 2>&1 | head -5; then
        echo "WARN: clang-tidy issues in $f"
    fi
done

# 4. Unit tests
echo "=== Tests ==="
if [ -f build-debug/dc_unit_tests ]; then
    if ! ctest --test-dir build-debug --output-on-failure -j$(nproc) 2>&1 | tail -10; then
        echo "FAIL: Tests failed"
        ERRORS=$((ERRORS + 1))
    fi
fi

# 5. Golden file check
echo "=== Golden Files ==="
if [ -d tests/golden ]; then
    for received in tests/golden/*.received.*; do
        [ -f "$received" ] || continue
        approved="${received/.received./.approved.}"
        if [ -f "$approved" ] && ! diff -q "$received" "$approved" > /dev/null 2>&1; then
            echo "FAIL: Golden file mismatch: $received vs $approved"
            ERRORS=$((ERRORS + 1))
        fi
    done
fi

if [ $ERRORS -gt 0 ]; then
    echo "=== $ERRORS verification failure(s) ==="
    exit 1
fi

echo "=== All checks passed ==="
exit 0
```

---

## Claude Code Hooks Configuration

### .claude/settings.json

```json
{
    "hooks": {
        "PostToolUse": [
            {
                "matcher": "Edit|Write",
                "hooks": [
                    {
                        "type": "command",
                        "command": "$CLAUDE_PROJECT_DIR/scripts/quick-check.sh"
                    }
                ]
            }
        ],
        "Stop": [
            {
                "hooks": [
                    {
                        "type": "command",
                        "command": "$CLAUDE_PROJECT_DIR/scripts/verify.sh"
                    }
                ]
            }
        ]
    }
}
```

### Hook Types

| Type | Use Case | Input | Output |
|------|----------|-------|--------|
| `command` | Deterministic checks (lint, build, test) | Tool use JSON on stdin | stdout shown to agent; non-zero exit blocks |
| `prompt` | Judgment calls (code review) | Single-turn LLM evaluation | LLM response shown to agent |
| `agent` | Complex verification (multi-step) | Multi-turn subagent with tools | Subagent result shown to agent |

For the test harness, `command` hooks are sufficient — all checks are deterministic.

### Hook Input Format

Hooks receive JSON on stdin:

```json
{
    "tool_name": "Edit",
    "tool_input": {
        "file_path": "/path/to/file.cpp",
        "old_string": "...",
        "new_string": "..."
    },
    "tool_output": "...",
    "stop_hook_active": false
}
```

### Stop Hook Infinite Loop Prevention

The Stop hook fires whenever the agent finishes responding. If the hook's output
causes the agent to respond again, the hook fires again. To prevent loops, check
`stop_hook_active`:

```bash
INPUT=$(cat)
if [ "$(echo "$INPUT" | jq -r '.stop_hook_active')" = "true" ]; then
    exit 0  # Already inside a stop hook, skip
fi
```

---

## CLAUDE.md Additions

Add to the project's `CLAUDE.md`:

```markdown
## Testing

### Running Tests

```bash
cmake --preset test             # configure with testing enabled
cmake --build --preset test     # build app + test executables
ctest --test-dir build-debug --output-on-failure -j$(nproc)
```

### Verification

IMPORTANT: Run `scripts/verify.sh` before declaring any task complete.
If the script fails, fix the issue and re-run until it passes.

### Test Conventions

- All new dc:: code must have corresponding unit tests in `tests/unit/`
- Test files follow `test_<class_name>.cpp` naming convention
- Use Catch2 `TEST_CASE` with descriptive string names
- Use `SECTION` blocks for shared setup within a test case
- Tag integration tests with `[integration]`, fuzz tests with `[fuzz]`
- Never commit code that fails `scripts/check_architecture.sh`
```

---

## Golden File Testing (ApprovalTests.cpp)

### Integration

```cmake
FetchContent_Declare(
    ApprovalTests
    GIT_REPOSITORY https://github.com/approvals/ApprovalTests.cpp.git
    GIT_TAG        v.10.13.0
)
FetchContent_MakeAvailable(ApprovalTests)

target_link_libraries(dc_integration_tests PRIVATE ApprovalTests::ApprovalTests)
```

### Workflow

1. Test produces output (e.g., serialised YAML session)
2. First run: output saved as `*.received.txt`, test fails (no approved file)
3. Developer reviews and copies to `*.approved.txt` to approve
4. Subsequent runs: output compared to approved file; test fails on mismatch

### Example

```cpp
#include <catch2/catch_test_macros.hpp>
#include <ApprovalTests/ApprovalTests.hpp>

TEST_CASE("Session round-trip preserves arrangement")
{
    auto project = createTestProject();
    addTrack(project, "Track 1");
    addClip(project, 0, 0, 44100);  // 1 second clip at position 0

    auto yaml = serializeSession(project);
    ApprovalTests::Approvals::verify(yaml);
    // Compares against test_session_roundtrip.Session_round-trip_preserves_arrangement.approved.txt
}
```

### Golden File Candidates

| Domain | What to Snapshot | File Pattern |
|--------|-----------------|-------------|
| Session YAML | Full project serialisation | `*.approved.yaml` |
| Audio config | Engine initialisation state | `*.approved.txt` |
| Coordinate transforms | Timeline position calculations | `*.approved.txt` |
| Plugin discovery | Parameter lists for test VST3s | `*.approved.txt` |

### Naming Convention

ApprovalTests generates file names from the test case and section names:

```
<SourceFileName>.<TestCaseName>.<SectionName>.approved.txt
```

Store approved files in `tests/golden/` and commit them to the repository.

---

## Regression Capture Workflow

### Purpose

Every bug found during AI agent sessions — or any other development — becomes a
permanent test case. Regression tests are never deleted.

### Location

```
tests/regression/
├── .gitkeep
├── issue_042_playhead_offset.cpp
├── issue_055_midi_note_off.cpp
└── issue_061_property_tree_listener.cpp
```

### Template

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

### Workflow

1. Bug is discovered (during agent session, testing, or user report)
2. Write a test that reproduces the bug (test should fail without the fix)
3. Fix the bug
4. Verify test passes
5. Commit test alongside fix — test is permanent

### CMake Integration

Add regression tests to the unit test executable:

```cmake
file(GLOB REGRESSION_TESTS tests/regression/*.cpp)
target_sources(dc_unit_tests PRIVATE ${REGRESSION_TESTS})
```

Using `file(GLOB ...)` here is acceptable because regression tests are only added,
never removed, and CMake re-globs on each configure.

---

## Spotify "Honk" Design Principle

The verification system follows Spotify's "Honk" pattern for background coding agents:

> The agent does not know what the verifiers do or how they work.

This means:
- Scripts are external to the agent prompt — the agent cannot modify them
- Non-zero exit from a hook blocks the agent and surfaces the error
- ~25% of sessions are vetoed by verifiers; ~50% of those self-correct
- The agent responds to verification failures by fixing issues, not by disabling checks

---

## Verification

1. `scripts/quick-check.sh` runs in ≤ 2 seconds on a single file edit
2. `scripts/verify.sh` runs in ≤ 30 seconds and catches:
   - Build failures
   - Architecture boundary violations
   - Test failures
   - Golden file mismatches
3. Claude Code hooks fire correctly on Edit/Write (PostToolUse) and session end (Stop)
4. Stop hook does not loop (checked via `stop_hook_active`)
5. Regression tests are included in `dc_unit_tests` and run with `ctest`
