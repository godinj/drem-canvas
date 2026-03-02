#!/usr/bin/env bash
# Capture default plugin states for the e2e-state-restore fixture.
# Run after building: ./tests/fixtures/e2e-state-restore/capture.sh [binary]
set -euo pipefail

BINARY="${1:-./build/DremCanvas_artefacts/Release/DremCanvas}"
FIXTURE_DIR="$(cd "$(dirname "$0")" && pwd)"

# Check prerequisites
for plugin in "/usr/lib/vst3/Vital.vst3" "$HOME/.vst3/yabridge/Kilohearts/kHs Gate.vst3"; do
    if [ ! -e "$plugin" ]; then
        echo "ERROR: plugin not found: $plugin"
        exit 1
    fi
done

# Create a temp fixture with empty state for capture
TMPFIXTURE=$(mktemp -d)
trap "rm -rf $TMPFIXTURE" EXIT

cp "$FIXTURE_DIR/session.yaml" "$TMPFIXTURE/"

# Create track YAMLs with empty state for capture
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

# Capture state from the empty-state fixture
OUTPUT=$(timeout 30 "$BINARY" --smoke --load "$TMPFIXTURE" --capture-plugin-state 2>/dev/null)

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
