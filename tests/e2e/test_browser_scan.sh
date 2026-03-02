#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"

# Check that at least one VST3 directory exists with plugins
HAS_PLUGINS=false
for dir in /usr/lib/vst3 "$HOME/.vst3"; do
    if [ -d "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
        HAS_PLUGINS=true
        break
    fi
done

if [ "$HAS_PLUGINS" = false ]; then
    echo "SKIP: no VST3 plugins found in standard paths"
    exit 0
fi

# Use isolated app data so scan starts fresh
export XDG_DATA_HOME="$(mktemp -d)"
trap "rm -rf $XDG_DATA_HOME" EXIT

run_with_display 120 "$BINARY" \
    --smoke \
    --browser-scan \
    --expect-known-plugins-gt 0

echo "PASS: browser scan found plugins"
