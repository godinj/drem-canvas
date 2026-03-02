#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-plugin-project}"

# Check plugin availability — skip if plugins are missing
check_plugin() {
    local path="$1"
    # Expand ~ to $HOME
    path="${path/#\~/$HOME}"
    if [ ! -e "$path" ]; then
        echo "SKIP: plugin not found: $path"
        exit 0
    fi
}

check_plugin "/usr/lib/vst3/Vital.vst3"
check_plugin "~/.vst3/yabridge/Kilohearts/kHs Gain.vst3"

run_with_display 30 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 2 \
    --expect-plugins 2

echo "PASS: project load with plugins"
