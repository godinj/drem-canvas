#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

# Check plugin availability
if [ ! -e "/usr/lib/vst3/Vital.vst3" ]; then
    echo "SKIP: Vital.vst3 not found"
    exit 0
fi

# Use isolated config to guarantee empty cache
export XDG_CONFIG_HOME="$(mktemp -d)"
trap "rm -rf $XDG_CONFIG_HOME" EXIT

run_with_display 60 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --scan-plugin 0 0 \
    --no-spatial-cache \
    --expect-spatial-params-gt 3

echo "PASS: spatial scan (cold, no cache)"
