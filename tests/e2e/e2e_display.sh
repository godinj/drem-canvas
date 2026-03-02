#!/usr/bin/env bash
# Shared helper for e2e tests: run a command with a display available.
#
# Usage:  source "$(dirname "$0")/e2e_display.sh"
#         run_with_display <timeout_secs> <command> [args...]

run_with_display() {
    local timeout_secs="$1"; shift
    if [ -n "${DISPLAY:-}" ]; then
        timeout "$timeout_secs" "$@"
    elif command -v xvfb-run >/dev/null 2>&1; then
        timeout "$timeout_secs" xvfb-run -a "$@"
    else
        echo "SKIP: no display available"
        exit 0
    fi
}
