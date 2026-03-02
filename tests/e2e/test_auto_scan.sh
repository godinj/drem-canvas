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

# Use isolated app data so there is no pre-existing pluginList.yaml.
# This forces the app to discover plugins from scratch at startup.
export XDG_DATA_HOME="$(mktemp -d)"
trap "rm -rf $XDG_DATA_HOME" EXIT

# Launch WITHOUT --browser-scan. The app should auto-scan because
# pluginList.yaml is empty (fresh XDG_DATA_HOME).
run_with_display 150 "$BINARY" \
    --smoke \
    --expect-known-plugins-gt 0

echo "PASS: auto-scan found plugins at startup (no manual scan trigger)"
