#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
PID_FILE="$DIR/wake_loop.pid"
LOG_FILE="$DIR/wake_loop.log"
WAKE_ENGINE="${WAKE_ENGINE:-kws}"

case "$WAKE_ENGINE" in
    kws)
        echo "kws polling wake is disabled because repeated card0 capture triggers rockchip-sai frame sync logs." >&2
        echo "Use ./record_kws_sample.sh 4 for one-shot KWS testing until the realtime wake path is fixed." >&2
        exit 1
        ;;
    kws-poll) LOOP="$DIR/kws_wake_loop.sh" ;;
    asr) LOOP="$DIR/wake_loop.sh" ;;
    *)
        echo "unknown WAKE_ENGINE: $WAKE_ENGINE" >&2
        exit 2
        ;;
esac

if [ -s "$PID_FILE" ]; then
    OLD_PID="$(cat "$PID_FILE" 2>/dev/null || true)"
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "wake_loop already running: pid=$OLD_PID"
        exit 0
    fi
fi

cd "$DIR"
{
    echo
    echo "===== $(date '+%Y-%m-%d %H:%M:%S') start wake engine=$WAKE_ENGINE ====="
} >>"$LOG_FILE"
nohup "$LOOP" >>"$LOG_FILE" 2>&1 &
PID="$!"
printf '%s\n' "$PID" >"$PID_FILE"
echo "wake_loop started: pid=$PID engine=$WAKE_ENGINE log=$LOG_FILE"
