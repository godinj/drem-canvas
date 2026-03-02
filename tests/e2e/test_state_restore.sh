#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-state-restore}"

# Check plugin availability — skip if plugins are missing
check_plugin() {
    local path="$1"
    path="${path/#\~/$HOME}"
    if [ ! -e "$path" ]; then
        echo "SKIP: plugin not found: $path"
        exit 0
    fi
}

check_plugin "/usr/lib/vst3/Vital.vst3"
check_plugin "~/.vst3/yabridge/Kilohearts/kHs Gate.vst3"

# Check that fixture has real state (not placeholder)
if grep -q '<CAPTURED_STATE>' "$FIXTURE/track-0.yaml" 2>/dev/null; then
    echo "SKIP: fixture state not captured — run capture.sh first"
    exit 0
fi

# Load project with saved state, then process 50 audio frames (~500ms).
# If setState() lifecycle is broken (missing setupProcessing before
# setActive), the audio thread will SIGSEGV during process() and
# this test will fail with a non-zero exit / signal.
run_with_display 30 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 2 \
    --expect-plugins 2 \
    --process-frames 50

echo "PASS: plugin state restore + audio processing survived"
