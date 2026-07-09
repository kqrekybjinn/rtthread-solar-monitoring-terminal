#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

WAKE_PHRASE="${WAKE_PHRASE:-小电小电小电}"
WAKE_SECONDS="${WAKE_SECONDS:-3}"
TURN_SECONDS="${TURN_SECONDS:-5}"
WAKE_COOLDOWN="${WAKE_COOLDOWN:-2}"
WAKE_ACK_TEXT="${WAKE_ACK_TEXT:-我在。}"

WAKE_WAV="$VOICE_DIR/wake_input_16k.wav"
WAKE_TEXT="$VOICE_DIR/wake_text.txt"
WAKE_ACK_WAV="$VOICE_DIR/wake_ack.wav"

normalize_text() {
    tr -d '[:space:]' \
        | sed 's/[，。！？、,.!?；;：“”"'\''（）()【】\[\]《》<>-]//g'
}

mkdir -p "$VOICE_DIR"

echo "wake_loop: waiting for '$WAKE_PHRASE'"
while :; do
    "$DIR/record_once.sh" "$WAKE_SECONDS" "$WAKE_WAV" >/dev/null
    "$ASR_PROGRAM" "$WAKE_WAV" >"$WAKE_TEXT" || true

    RAW="$(cat "$WAKE_TEXT" 2>/dev/null || true)"
    NORM="$(printf '%s' "$RAW" | normalize_text)"
    TARGET="$(printf '%s' "$WAKE_PHRASE" | normalize_text)"

    if [ -n "$NORM" ]; then
        echo "wake_loop: heard '$RAW'"
    fi

    case "$NORM" in
        *"$TARGET"*)
            echo "wake_loop: wake matched"
            "$TTS_PROGRAM" "$WAKE_ACK_TEXT" "$WAKE_ACK_WAV" >/dev/null || true
            "$DIR/play_wav.sh" "$WAKE_ACK_WAV" >/dev/null || true
            sleep "$WAKE_COOLDOWN"
            "$DIR/ask.sh" "$TURN_SECONDS"
            sleep "$WAKE_COOLDOWN"
            echo "wake_loop: waiting for '$WAKE_PHRASE'"
            ;;
    esac
done
