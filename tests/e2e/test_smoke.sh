#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
run_with_display 15 "$BINARY" --smoke
echo "PASS: smoke test (clean launch + exit)"
