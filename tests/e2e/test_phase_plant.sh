#!/usr/bin/env bash
# End-to-end test: Phase Plant (yabridge-bridged Kilohearts VST3 synth)
#
# Validates the full Phase Plant lifecycle:
#   1. Load from fixture (verifies yabridge scan serialization fix)
#   2. Process audio frames (verifies VST3 process() on audio thread)
#
# Prerequisites:
#   - Phase Plant must be installed at ~/.vst3/yabridge/Kilohearts/Phase Plant.vst3
#   - yabridge must be functional (chainloader .so inside the bundle)
#   - A display must be available (xvfb-run fallback via run_with_display)
#
# Timeouts are generous because yabridge loads require Wine bridge
# setup + 500ms settle delay.

set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-phase-plant}"

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
# Use temp XDG_CONFIG_HOME so ProbeCache starts fresh.  Otherwise a
# previous crash could leave Phase Plant marked as blocked.

TMPDIR_CONFIG="$(mktemp -d)"
export XDG_CONFIG_HOME="$TMPDIR_CONFIG"
trap "rm -rf $TMPDIR_CONFIG" EXIT

# ── Test: load Phase Plant from fixture + process audio ──────────────
# --expect-plugins 1 ensures Phase Plant was not blocked by the scanner
# and that createPluginAsync() successfully instantiated the plugin.
# --process-frames 30 verifies Phase Plant's process() runs on the
# audio thread without crashing (~300ms of audio at 44.1kHz).

run_with_display 60 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 1 \
    --expect-plugins 1 \
    --process-frames 30

echo "PASS: Phase Plant load + audio processing"
