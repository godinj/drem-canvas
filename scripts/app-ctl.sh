#!/usr/bin/env bash
# app-ctl.sh — PID-file scoped app launcher for worktree isolation.
# Usage: scripts/app-ctl.sh start | stop

set -euo pipefail

WORKTREE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PID_FILE="$WORKTREE_ROOT/.drem.pid"

stop_existing()
{
    if [[ -f "$PID_FILE" ]]; then
        local pid
        pid=$(<"$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            # Wait up to 3 seconds for graceful exit
            for _ in 1 2 3 4 5 6; do
                kill -0 "$pid" 2>/dev/null || break
                sleep 0.5
            done
            # Force kill if still alive
            if kill -0 "$pid" 2>/dev/null; then
                kill -9 "$pid" 2>/dev/null || true
            fi
        fi
        rm -f "$PID_FILE"
    fi
}

cmd_start()
{
    stop_existing

    local platform
    platform="$(uname -s)"

    case "$platform" in
        Darwin)
            open "$WORKTREE_ROOT/build/DremCanvas_artefacts/Release/Drem Canvas.app"
            # 'open' detaches; grab the PID of the launched app
            sleep 0.5
            local pid
            pid=$(pgrep -n "DremCanvas" 2>/dev/null || true)
            if [[ -n "$pid" ]]; then
                echo "$pid" > "$PID_FILE"
            fi
            ;;
        Linux)
            WINEESYNC=0 "$WORKTREE_ROOT/build/DremCanvas" &
            echo $! > "$PID_FILE"
            ;;
        *)
            echo "Unsupported platform: $platform" >&2
            exit 1
            ;;
    esac
}

cmd_stop()
{
    stop_existing
}

case "${1:-}" in
    start) cmd_start ;;
    stop)  cmd_stop  ;;
    *)
        echo "Usage: $0 start|stop" >&2
        exit 1
        ;;
esac
