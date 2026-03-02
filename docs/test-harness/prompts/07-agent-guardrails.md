# Agent: Agent Guardrails

You are working on the `feature/test-harness` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 9 of the test harness: create verification scripts, configure
Claude Code hooks, and update CLAUDE.md with testing requirements.

## Context

Read these specs before starting:

- `docs/test-harness/04-agent-management.md` (full design — scripts, hooks, golden files, regression workflow)
- `docs/test-harness/03-architecture-guards.md` (architecture check script design)
- `CLAUDE.md` (current project instructions — you will add testing sections)
- `scripts/check_architecture.sh` (created in Phase 2 — verify it exists)

## Prerequisites

Phase 1 (CMake infrastructure) and Phase 2 (architecture scripts) must be completed.
Test targets (`dc_unit_tests`) must exist and have tests registered.

## Deliverables

### 1. scripts/quick-check.sh

Tier 1 verification script. Runs after every file Edit/Write via PostToolUse hook.
Must complete in ≤ 2 seconds.

Receives JSON on stdin with `tool_name`, `tool_input.file_path`, etc.

Checks:
1. Architecture boundary grep on the changed file (dc:: files only)
2. Real-time safety check on engine processor files (if changed)
3. Optional: syntax check via `clang-check` if `compile_commands.json` exists

See `docs/test-harness/04-agent-management.md` for the exact script implementation.

Make executable: `chmod +x scripts/quick-check.sh`

### 2. scripts/verify.sh

Tier 2 verification script. Runs when agent session ends via Stop hook.
Budget: ≤ 30 seconds.

Receives JSON on stdin. Must check `stop_hook_active` to prevent infinite loops:
```bash
INPUT=$(cat)
if [ "$(echo "$INPUT" | jq -r '.stop_hook_active')" = "true" ]; then
    exit 0
fi
```

Checks:
1. Full build: `cmake --build --preset test`
2. Architecture check: `scripts/check_architecture.sh`
3. Static analysis on changed files (clang-tidy, if available)
4. Unit tests: `ctest --test-dir build-debug --output-on-failure`
5. Golden file comparisons (if `tests/golden/` contains `.received` files)

Exit non-zero if any check fails. Print summary of failures.

Make executable: `chmod +x scripts/verify.sh`

### 3. .claude/settings.json — Hooks Configuration

Create or update `.claude/settings.json` with hooks:

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

If `.claude/settings.json` already exists, merge the hooks configuration without
overwriting existing settings.

### 4. CLAUDE.md Testing Additions

Add a `## Testing` section to `CLAUDE.md` with:

- How to run tests (`cmake --preset test`, `cmake --build --preset test`,
  `ctest --test-dir build-debug`)
- Verification requirement: run `scripts/verify.sh` before declaring tasks complete
- Test conventions:
  - All new dc:: code must have unit tests in `tests/unit/`
  - Test files: `test_<class_name>.cpp`
  - Use Catch2 `TEST_CASE` with descriptive string names
  - Use `SECTION` blocks for shared setup
  - Tag integration tests with `[integration]`
  - Never commit code that fails `scripts/check_architecture.sh`
- Regression test workflow: bug-fix tests go in `tests/regression/`

### 5. tests/regression/.gitkeep

Ensure `tests/regression/` directory exists with a `.gitkeep` file.

### 6. Regression Test Template

Create `tests/regression/README.md` with the template and workflow:
- File naming: `issue_NNN_short_description.cpp`
- Template structure: bug description, cause, fix, test case
- Workflow: reproduce → test → fix → verify → commit

## Important

- The `quick-check.sh` script must be fast (≤ 2s). Do not run full builds or
  full test suites in this script.
- The `verify.sh` script must handle the `stop_hook_active` check to prevent
  infinite loops when the Stop hook triggers.
- Both scripts must work on Linux (the primary development platform).
- Scripts must handle missing dependencies gracefully (e.g., if `clang-tidy` is
  not installed, skip static analysis with a warning, do not fail).
- The CLAUDE.md additions must be consistent with the existing document style
  and not duplicate information already present.
- Test that `scripts/quick-check.sh` exits 0 on a valid dc:: file and exits
  non-zero when a JUCE reference is intentionally added.

## Conventions

- Shell scripts: POSIX-compatible (`#!/usr/bin/env bash`, `set -euo pipefail`)
- JSON: standard formatting, no trailing commas
- CLAUDE.md: follow existing section and formatting conventions
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure`
