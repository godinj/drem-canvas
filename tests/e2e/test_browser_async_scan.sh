#!/usr/bin/env bash
# E2E: Async browser scan exercises the progress bar UI lifecycle.
#
# Validates that BrowserWidget::startAsyncScan() correctly shows the progress
# bar during scanning and hides it upon completion.  Uses isolated XDG
# directories so the scan starts with no cached state.
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

# Isolate both data and config to ensure fresh scan (no pluginList.yaml, no probeCache)
TMPDIR_DATA="$(mktemp -d)"
TMPDIR_CONFIG="$(mktemp -d)"
export XDG_DATA_HOME="$TMPDIR_DATA"
export XDG_CONFIG_HOME="$TMPDIR_CONFIG"
trap "rm -rf $TMPDIR_DATA $TMPDIR_CONFIG" EXIT

run_with_display 150 "$BINARY" \
    --smoke \
    --browser-async-scan \
    --expect-known-plugins-gt 0

echo "PASS: async browser scan exercised progress bar lifecycle"
