#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-scan-project}"

# Check plugin availability
if [ ! -e "/usr/lib/vst3/Vital.vst3" ]; then
    echo "SKIP: Vital.vst3 not found"
    exit 0
fi

# Use isolated config for reproducibility
export XDG_CONFIG_HOME="$(mktemp -d)"
trap "rm -rf $XDG_CONFIG_HOME" EXIT

# First run populates the cache
timeout 60 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --scan-plugin 0 0

# Second run should hit cache and still produce results
timeout 15 xvfb-run -a "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --scan-plugin 0 0 \
    --expect-spatial-params-gt 3

echo "PASS: spatial scan (warm, from cache)"
