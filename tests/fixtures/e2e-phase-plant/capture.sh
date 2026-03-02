#!/usr/bin/env bash
# Capture default Phase Plant plugin state for the e2e-phase-plant fixture.
# Run after building: ./tests/fixtures/e2e-phase-plant/capture.sh [binary]
set -euo pipefail
source "$(dirname "$0")/../../e2e/e2e_display.sh"

BINARY="${1:-./build/DremCanvas}"
FIXTURE_DIR="$(cd "$(dirname "$0")" && pwd)"

# Check Phase Plant is installed
PLUGIN_PATH="$HOME/.vst3/yabridge/Kilohearts/Phase Plant.vst3"
if [ ! -e "$PLUGIN_PATH" ]; then
    echo "ERROR: Phase Plant not found: $PLUGIN_PATH"
    exit 1
fi

# Create a temp fixture with empty state for capture
TMPFIXTURE=$(mktemp -d)
trap "rm -rf $TMPFIXTURE" EXIT

cp "$FIXTURE_DIR/session.yaml" "$TMPFIXTURE/"

# Create track YAML with empty state for capture
for f in "$FIXTURE_DIR"/track-*.yaml; do
    basename=$(basename "$f")
    # Replace the state value with empty string for capture source
    python3 -c "
import re, sys
content = open('$f').read()
content = re.sub(r'state: \"[^\"]*\"', 'state: \"\"', content)
open('$TMPFIXTURE/$basename', 'w').write(content)
"
done

# Capture state — Phase Plant is a yabridge plugin so give it more time
OUTPUT=$(run_with_display 60 "$BINARY" --smoke --load "$TMPFIXTURE" --capture-plugin-state 2>/dev/null)

# Parse and inject into track YAMLs using python to handle large state blobs
# (sed fails with "Argument list too long" on large base64 strings)
count=0
while IFS= read -r line; do
    if [[ "$line" =~ PLUGIN_STATE\ track=([0-9]+)\ slot=([0-9]+)\ state=(.*) ]]; then
        track="${BASH_REMATCH[1]}"
        state="${BASH_REMATCH[3]}"
        track_file="$FIXTURE_DIR/track-${track}.yaml"

        if [ -f "$track_file" ]; then
            # Write state to temp file to avoid argument length limits
            state_file=$(mktemp)
            printf '%s' "$state" > "$state_file"

            python3 -c "
import re
state = open('$state_file').read()
track = open('$track_file').read()
# Replace any existing state (placeholder or captured)
track = re.sub(r'state: \"[^\"]*\"', 'state: \"' + state + '\"', track)
open('$track_file', 'w').write(track)
"
            rm -f "$state_file"
            echo "Updated $track_file with ${#state} chars of state"
            count=$((count + 1))
        fi
    fi
done <<< "$OUTPUT"

if [ "$count" -eq 0 ]; then
    echo "ERROR: no plugin states captured"
    exit 1
fi

echo "Done. Captured $count plugin state(s)."
