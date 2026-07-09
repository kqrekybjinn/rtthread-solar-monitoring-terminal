#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
PID_FILE="$DIR/wake_loop.pid"

if [ ! -s "$PID_FILE" ]; then
    echo "wake_loop not running"
    exit 0
fi

PID="$(cat "$PID_FILE" 2>/dev/null || true)"
if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
    kill "$PID" 2>/dev/null || true
    sleep 1
    kill -9 "$PID" 2>/dev/null || true
    echo "wake_loop stopped: pid=$PID"
else
    echo "wake_loop pid file was stale"
fi

rm -f "$PID_FILE"
pkill -f sherpa-onnx-keyword-spotter 2>/dev/null || true
killall sherpa-onnx-keyword-spotter 2>/dev/null || true
killall sherpa-onnx-keyword-spotter-microphone 2>/dev/null || true
