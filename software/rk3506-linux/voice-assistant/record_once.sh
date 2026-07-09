#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

SECONDS_ARG="${1:-$RECORD_SECONDS}"
OUT="${2:-$VOICE_DIR/last_input_16k.wav}"

mkdir -p "$VOICE_DIR"
"$DIR/voice_pcm_record" "$OUT" "$SECONDS_ARG" "$CAPTURE_DEV" "$RECORD_GAIN"
echo "$OUT"
