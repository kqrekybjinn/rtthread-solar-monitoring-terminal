#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

SECONDS_ARG="${1:-$RECORD_SECONDS}"
IN_WAV="$VOICE_DIR/local_asr_input_16k.wav"
TEXT_OUT="$VOICE_DIR/local_asr_text.txt"
ASR="${LOCAL_ASR_PROGRAM:-/userdata/local-asr/local_asr.sh}"

mkdir -p "$VOICE_DIR"
"$DIR/record_once.sh" "$SECONDS_ARG" "$IN_WAV" >/dev/null
"$ASR" "$IN_WAV" | tee "$TEXT_OUT"
