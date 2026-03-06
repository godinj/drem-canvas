#!/usr/bin/env bash
# Tier 2 verification — runs when the agent session ends via Stop hook.
# Budget: <= 30 seconds.
#
# Receives JSON on stdin. Checks stop_hook_active to prevent infinite loops.
# Exits non-zero if any check fails, printing a summary of failures.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

INPUT=$(cat)

# Prevent infinite loop: skip if already inside a Stop hook
if [ "$(echo "$INPUT" | jq -r '.stop_hook_active // false')" = "true" ]; then
    exit 0
fi

ERRORS=0
WARNINGS=0

#==============================================================================
# 1. Full build
#==============================================================================

echo "=== Building (cmake --build --preset test) ==="
if command -v cmake >/dev/null 2>&1; then
    # Configure if build directory does not exist
    if [ ! -d build-debug ]; then
        echo "Configuring test preset..."
        if ! cmake --preset test 2>&1 | tail -5; then
            echo "FAIL: CMake configure failed"
            ERRORS=$((ERRORS + 1))
        fi
    fi

    if ! cmake --build --preset test 2>&1 | tail -20; then
        echo "FAIL: Build failed"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "SKIP: cmake not found"
    WARNINGS=$((WARNINGS + 1))
fi

#==============================================================================
# 2. Architecture check
#==============================================================================

echo ""
echo "=== Architecture (scripts/check_architecture.sh) ==="
if [ -x scripts/check_architecture.sh ]; then
    if ! scripts/check_architecture.sh; then
        echo "FAIL: Architecture violations found"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "SKIP: scripts/check_architecture.sh not found or not executable"
    WARNINGS=$((WARNINGS + 1))
fi

#==============================================================================
# 3. Static analysis on changed files (clang-tidy, if available)
#==============================================================================

echo ""
echo "=== Static Analysis ==="
if command -v clang-tidy >/dev/null 2>&1 && [ -f build-debug/compile_commands.json ]; then
    CHANGED=$(git diff --name-only HEAD 2>/dev/null | grep -E '\.cpp$' | grep 'src/dc/' || true)
    if [ -n "$CHANGED" ]; then
        for f in $CHANGED; do
            if [ -f "$f" ]; then
                echo "  Checking $f ..."
                if ! clang-tidy "$f" -p build-debug/ --quiet 2>&1 | head -10; then
                    echo "WARN: clang-tidy issues in $f"
                    WARNINGS=$((WARNINGS + 1))
                fi
            fi
        done
    else
        echo "  No changed dc:: .cpp files to analyze."
    fi
else
    echo "SKIP: clang-tidy not available or compile_commands.json missing"
fi

#==============================================================================
# 4. Unit tests
#==============================================================================

echo ""
echo "=== Tests (ctest --test-dir build-debug) ==="
if command -v ctest >/dev/null 2>&1 && [ -d build-debug ]; then
    if ! ctest --test-dir build-debug --output-on-failure -j"$(nproc 2>/dev/null || echo 4)" 2>&1 | tail -15; then
        echo "FAIL: Tests failed"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "SKIP: ctest not available or build-debug directory missing"
    WARNINGS=$((WARNINGS + 1))
fi

#==============================================================================
# 5. Golden file comparisons
#==============================================================================

echo ""
echo "=== Golden Files ==="
if [ -d tests/golden ]; then
    GOLDEN_ERRORS=0
    for received in tests/golden/*.received.*; do
        [ -f "$received" ] || continue
        approved="${received/.received./.approved.}"
        if [ -f "$approved" ]; then
            if ! diff -q "$received" "$approved" > /dev/null 2>&1; then
                echo "FAIL: Golden file mismatch:"
                echo "  received: $received"
                echo "  approved: $approved"
                diff --unified=3 "$approved" "$received" | head -30 || true
                GOLDEN_ERRORS=$((GOLDEN_ERRORS + 1))
            fi
        else
            echo "WARN: No approved file for $received"
            echo "  Review and copy to $approved to approve."
            WARNINGS=$((WARNINGS + 1))
        fi
    done

    if [ $GOLDEN_ERRORS -gt 0 ]; then
        echo "FAIL: $GOLDEN_ERRORS golden file mismatch(es)"
        ERRORS=$((ERRORS + 1))
    else
        echo "  No golden file mismatches."
    fi
else
    echo "  No tests/golden/ directory found."
fi

#==============================================================================
# Summary
#==============================================================================

echo ""
echo "========================================"
if [ $ERRORS -gt 0 ]; then
    echo "=== FAILED: $ERRORS check(s) failed, $WARNINGS warning(s) ==="
    exit 1
fi

if [ $WARNINGS -gt 0 ]; then
    echo "=== PASSED with $WARNINGS warning(s) ==="
else
    echo "=== All checks passed ==="
fi
exit 0
