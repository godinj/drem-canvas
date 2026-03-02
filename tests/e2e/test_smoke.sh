#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/DremCanvas}"
timeout 15 xvfb-run -a "$BINARY" --smoke
echo "PASS: smoke test (clean launch + exit)"
