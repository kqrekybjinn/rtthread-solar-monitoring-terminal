#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

KWS_ROOT="${KWS_ROOT:-$VOICE_DIR/kws}"
KWS_MODEL_DIR="${KWS_MODEL_DIR:-$KWS_ROOT/model/sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20}"
KWS_BIN="${KWS_BIN:-$KWS_ROOT/bin/sherpa-onnx-keyword-spotter}"
KWS_KEYWORDS="${KWS_KEYWORDS:-$KWS_ROOT/keywords.txt}"
KWS_TOKENS="${KWS_TOKENS:-$KWS_MODEL_DIR/tokens.txt}"
KWS_ENCODER="${KWS_ENCODER:-$KWS_MODEL_DIR/encoder-epoch-13-avg-2-chunk-16-left-64.int8.onnx}"
KWS_DECODER="${KWS_DECODER:-$KWS_MODEL_DIR/decoder-epoch-13-avg-2-chunk-16-left-64.onnx}"
KWS_JOINER="${KWS_JOINER:-$KWS_MODEL_DIR/joiner-epoch-13-avg-2-chunk-16-left-64.int8.onnx}"
KWS_THREADS="${KWS_THREADS:-2}"
KWS_SCORE="${KWS_SCORE:-2.0}"
KWS_THRESHOLD="${KWS_THRESHOLD:-0.45}"
KWS_COOLDOWN="${KWS_COOLDOWN:-2}"
KWS_RECORD_SECONDS="${KWS_RECORD_SECONDS:-2}"
AWAKE_WAV="${AWAKE_WAV:-$VOICE_DIR/prompts/awake.wav}"

LOG_HIT="$VOICE_DIR/last_wake.txt"
KWS_WAV="$VOICE_DIR/kws_input.wav"
KWS_OUT="$VOICE_DIR/kws_last_output.txt"

if [ ! -x "$KWS_BIN" ]; then
    echo "kws_wake_loop: missing executable: $KWS_BIN" >&2
    exit 1
fi

for f in \
    "$KWS_TOKENS" \
    "$KWS_ENCODER" \
    "$KWS_DECODER" \
    "$KWS_JOINER" \
    "$KWS_KEYWORDS" \
    "$AWAKE_WAV"; do
    if [ ! -s "$f" ]; then
        echo "kws_wake_loop: missing file: $f" >&2
        exit 1
    fi
done

echo "kws_wake_loop: polling local KWS keywords from $KWS_KEYWORDS"

while :; do
    "$DIR/record_once.sh" "$KWS_RECORD_SECONDS" "$KWS_WAV" >/dev/null 2>&1 || {
        echo "kws_wake_loop: record failed"
        sleep 1
        continue
    }

    "$KWS_BIN" \
        --print-args=false \
        --tokens="$KWS_TOKENS" \
        --encoder="$KWS_ENCODER" \
        --decoder="$KWS_DECODER" \
        --joiner="$KWS_JOINER" \
        --keywords-file="$KWS_KEYWORDS" \
        --model-type=zipformer2 \
        --num-threads="$KWS_THREADS" \
        --keywords-score="$KWS_SCORE" \
        --keywords-threshold="$KWS_THRESHOLD" \
        "$KWS_WAV" >"$KWS_OUT" 2>&1 || true

    if grep -q '"keyword"' "$KWS_OUT"; then
        date '+%Y-%m-%d %H:%M:%S' >"$LOG_HIT"
        grep '"keyword"' "$KWS_OUT" >>"$LOG_HIT" || true
        cat "$LOG_HIT"
        "$DIR/play_wav.sh" "$AWAKE_WAV" >/dev/null 2>&1 || true
        sleep "$KWS_COOLDOWN"
    else
        tail -n 3 "$KWS_OUT" | grep -E 'Audio duration|Elapsed|RTF' || true
    fi
done
