#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
PID_FILE="$DIR/wake_loop.pid"
LOG_FILE="$DIR/wake_loop.log"

if [ -s "$PID_FILE" ]; then
    PID="$(cat "$PID_FILE" 2>/dev/null || true)"
    if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
        echo "wake_loop running: pid=$PID"
    else
        echo "wake_loop not running: stale pid=$PID"
    fi
else
    echo "wake_loop not running"
fi

if [ -f "$LOG_FILE" ]; then
    echo "last log lines:"
    tail -n 10 "$LOG_FILE"
fi
