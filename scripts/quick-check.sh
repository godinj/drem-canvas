#!/usr/bin/env bash
# Tier 1 verification — runs after every Edit/Write via PostToolUse hook.
# Must complete in <= 2 seconds.
#
# Receives JSON on stdin with tool_name, tool_input.file_path, etc.
# Exits non-zero to surface errors to the agent.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

INPUT=$(cat)
TOOL=$(echo "$INPUT" | jq -r '.tool_name // empty')
FILE=$(echo "$INPUT" | jq -r '.tool_input.file_path // .tool_input.command // empty' | head -1)

# Only check on file modifications
case "$TOOL" in
    Edit|Write) ;;
    *) exit 0 ;;
esac

# Bail if FILE is empty or not a real file path
if [ -z "$FILE" ] || [ ! -f "$FILE" ]; then
    exit 0
fi

ERRORS=0

#==============================================================================
# 1. Real-time safety (engine processor files only)
#
# *Processor.cpp files in src/engine/ must not contain heap allocation,
# blocking, or I/O. Override with '// RT-safe:' or '// not on audio thread'.
#==============================================================================

if echo "$FILE" | grep -q 'src/engine/.*Processor\.cpp'; then
    RT_FORBIDDEN='(^|[^a-zA-Z_])(new |delete |malloc|free|realloc|calloc)[^a-zA-Z_]'
    RT_FORBIDDEN="$RT_FORBIDDEN|std::mutex|lock_guard|unique_lock|condition_variable"
    RT_FORBIDDEN="$RT_FORBIDDEN|std::cout|std::cerr|printf|fprintf|fopen|fwrite"
    RT_FORBIDDEN="$RT_FORBIDDEN|std::thread|pthread_create"

    hits=$(grep -n -E "$RT_FORBIDDEN" "$FILE" 2>/dev/null \
           | grep -v '// RT-safe:' \
           | grep -v '// not on audio thread' \
           || true)

    if [ -n "$hits" ]; then
        echo "RT-SAFETY VIOLATION: Forbidden pattern in $FILE"
        echo "$hits"
        ERRORS=$((ERRORS + 1))
    fi
fi

#==============================================================================
# 2. Syntax check (compile only, no link)
#
# Uses compile_commands.json for correct flags. Only runs if clang-check
# is available and compile_commands.json exists. Failure here is a warning,
# not a hard error — avoids blocking on missing tooling.
#==============================================================================

if echo "$FILE" | grep -qE '\.(cpp|h)$'; then
    if [ -f build-debug/compile_commands.json ] && command -v clang-check >/dev/null 2>&1; then
        if ! clang-check "$FILE" -p build-debug/ 2>&1 | head -20; then
            echo "WARN: Syntax check found issues in $FILE"
            # Syntax check is advisory — don't increment ERRORS
        fi
    fi
fi

if [ $ERRORS -gt 0 ]; then
    echo "=== quick-check: $ERRORS violation(s) ==="
    exit 1
fi

exit 0
