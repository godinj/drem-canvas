#!/usr/bin/env bash
# E2E: Phase Plant MIDI routing — verify patch produces non-silent audio
#
# Loads a MIDI track with 4 bars of quarter-note C4s routed through
# Phase Plant, bounces offline, then checks the output is non-silent.
#
# Prerequisites:
#   - Phase Plant must be installed and loadable (not blocked by ProbeCache)
#   - yabridge must be functional
#   - A display must be available (xvfb-run fallback via run_with_display)
set -euo pipefail
source "$(dirname "$0")/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE="${2:-tests/fixtures/e2e-phase-plant-routing}"

# ── Prerequisite checks ──────────────────────────────────────────────

PHASE_PLANT_PATH="$HOME/.vst3/yabridge/Kilohearts/Phase Plant.vst3"

if [ ! -d "$PHASE_PLANT_PATH" ]; then
    echo "SKIP: Phase Plant not found: $PHASE_PLANT_PATH"
    exit 0
fi

CHAINLOADER="$PHASE_PLANT_PATH/Contents/x86_64-linux/Phase Plant.so"
if [ ! -f "$CHAINLOADER" ]; then
    echo "SKIP: yabridge chainloader not found: $CHAINLOADER"
    exit 0
fi

# ── Pre-flight: verify Phase Plant actually loads ────────────────────
# Run a quick smoke load to check the plugin instantiates.  If the
# ProbeCache has Phase Plant blocked, the plugin count will be 0 and
# we skip rather than crash during bounce.

PREFLIGHT_OUT=$(run_with_display 60 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 1 \
    --process-frames 5 2>&1 || true)

if echo "$PREFLIGHT_OUT" | grep -q "skipping blocked module.*Phase Plant"; then
    echo "SKIP: Phase Plant is blocked by ProbeCache"
    exit 0
fi

if echo "$PREFLIGHT_OUT" | grep -q "failed to load module for plugin"; then
    echo "SKIP: Phase Plant failed to load"
    exit 0
fi

# ── Bounce 4 bars ────────────────────────────────────────────────────

BOUNCE_OUT="$(mktemp --suffix=.wav)"
trap "rm -f $BOUNCE_OUT" EXIT

run_with_display 90 "$BINARY" \
    --smoke \
    --load "$FIXTURE" \
    --expect-tracks 1 \
    --expect-plugins 1 \
    --bounce "$BOUNCE_OUT" \
    --bounce-bars 4

# ── Verify output ────────────────────────────────────────────────────

if [ ! -f "$BOUNCE_OUT" ]; then
    echo "FAIL: bounce output not created"
    exit 1
fi

FILE_SIZE=$(stat -c%s "$BOUNCE_OUT")

if [ "$FILE_SIZE" -lt 100000 ]; then
    echo "FAIL: bounce output too small ($FILE_SIZE bytes)"
    exit 1
fi

if command -v sox >/dev/null 2>&1; then
    RMS=$(sox "$BOUNCE_OUT" -n stat 2>&1 | grep "RMS.*amplitude" | head -1 | awk '{print $NF}')
    if python3 -c "import sys; sys.exit(0 if float('$RMS') > 0.001 else 1)" 2>/dev/null; then
        echo "PASS: Phase Plant routing — non-silent output (RMS=$RMS)"
    else
        echo "FAIL: Phase Plant routing — output is silent (RMS=$RMS)"
        exit 1
    fi
else
    echo "PASS: Phase Plant routing — bounce output created ($FILE_SIZE bytes, sox not available for RMS check)"
fi
