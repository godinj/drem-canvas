#!/usr/bin/env bash
# End-to-end test: Phase Plant appears in browser scan results (not blocked)
#
# Validates that the yabridge scan serialization fix prevents Phase Plant
# from being marked as blocked during a full browser scan.  Uses isolated
# XDG directories to ensure a fresh scan with no cached state.
#
# Prerequisites:
#   - Phase Plant must be installed at ~/.vst3/yabridge/Kilohearts/Phase Plant.vst3
#   - yabridge must be functional (chainloader .so inside the bundle)
#   - A display must be available (xvfb-run fallback via run_with_display)

set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"

# ── Prerequisite checks ──────────────────────────────────────────────

PHASE_PLANT_PATH="$HOME/.vst3/yabridge/Kilohearts/Phase Plant.vst3"

if [ ! -d "$PHASE_PLANT_PATH" ]; then
    echo "SKIP: Phase Plant not found: $PHASE_PLANT_PATH"
    exit 0
fi

# Verify yabridge chainloader is present inside the bundle
CHAINLOADER="$PHASE_PLANT_PATH/Contents/x86_64-linux/Phase Plant.so"
if [ ! -f "$CHAINLOADER" ]; then
    echo "SKIP: yabridge chainloader not found: $CHAINLOADER"
    exit 0
fi

# ── Environment isolation ────────────────────────────────────────────
# Use temp directories for both XDG_DATA_HOME (pluginList.yaml) and
# XDG_CONFIG_HOME (probeCache.yaml) to guarantee a completely fresh scan
# with no cached state from previous runs.

TMPDIR_DATA="$(mktemp -d)"
TMPDIR_CONFIG="$(mktemp -d)"
export XDG_DATA_HOME="$TMPDIR_DATA"
export XDG_CONFIG_HOME="$TMPDIR_CONFIG"
trap "rm -rf $TMPDIR_DATA $TMPDIR_CONFIG" EXIT

# ── Test: Phase Plant found in browser scan ──────────────────────────
# --browser-scan triggers a full scan of all VST3 directories.
# --expect-known-plugins-gt 0 verifies that at least one plugin was found.
# --expect-plugin-name "Phase Plant" verifies Phase Plant specifically
# was not blocked and appears in the scan results.
#
# Timeout is 120s because yabridge scans are slow: each bundle requires
# Wine bridge setup + 500ms settle delay.

run_with_display 120 "$BINARY" \
    --smoke \
    --browser-scan \
    --expect-known-plugins-gt 0 \
    --expect-plugin-name "Phase Plant"

echo "PASS: Phase Plant found in browser scan"
