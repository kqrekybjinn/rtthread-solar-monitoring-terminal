#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
RUNTIME="${LOCAL_TTS_DIR:-$DIR/local_tts_espeak}"
ESPEAK="${LOCAL_TTS_ESPEAK:-$RUNTIME/bin/espeak-ng}"
DATA_ROOT="${LOCAL_TTS_DATA_ROOT:-$RUNTIME/share}"
VOICE="${LOCAL_TTS_VOICE:-cmn}"
SPEED="${LOCAL_TTS_SPEED:-125}"
AMP="${LOCAL_TTS_AMPLITUDE:-90}"
PITCH="${LOCAL_TTS_PITCH:-28}"
GAP="${LOCAL_TTS_WORD_GAP:-8}"

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 text-or-text-file [output.wav]" >&2
    exit 2
fi

INPUT="$1"
OUT="${2:-$DIR/reply.wav}"
TMP="/tmp/local_tts_input_$$.txt"
trap 'rm -f "$TMP"' EXIT INT TERM

if [ -f "$INPUT" ]; then
    cp "$INPUT" "$TMP"
else
    printf '%s\n' "$INPUT" >"$TMP"
fi

# Keep the robot voice calm: strip markdown/control characters and limit long replies.
sed -i 's/[#*_`>|]//g; s/[[:cntrl:]]//g' "$TMP"
if [ "$(wc -c <"$TMP")" -gt 260 ]; then
    head -c 260 "$TMP" >"$TMP.short"
    printf '。\n' >>"$TMP.short"
    mv "$TMP.short" "$TMP"
fi

if [ ! -x "$ESPEAK" ]; then
    echo "local_tts: missing executable: $ESPEAK" >&2
    exit 1
fi

if [ ! -d "$DATA_ROOT/espeak-ng-data" ]; then
    echo "local_tts: missing data directory: $DATA_ROOT/espeak-ng-data" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT")"
"$ESPEAK" --path="$DATA_ROOT" -v "$VOICE" -s "$SPEED" -a "$AMP" -p "$PITCH" -g "$GAP" -w "$OUT" -f "$TMP" >/tmp/local_tts_espeak_$$.log 2>&1 || {
    cat /tmp/local_tts_espeak_$$.log >&2
    rm -f /tmp/local_tts_espeak_$$.log
    exit 1
}
rm -f /tmp/local_tts_espeak_$$.log

if [ ! -s "$OUT" ]; then
    echo "local_tts: output wav is empty: $OUT" >&2
    exit 1
fi

printf '%s\n' "$OUT"
