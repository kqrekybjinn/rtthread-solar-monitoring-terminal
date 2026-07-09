#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

WAV="${1:-$VOICE_DIR/wake_sample.wav}"
THRESHOLD="${2:-$KWS_THRESHOLD}"
SCORE="${3:-$KWS_SCORE}"

if [ ! -s "$WAV" ]; then
    echo "usage: $0 sample.wav [threshold] [score]" >&2
    echo "missing sample: $WAV" >&2
    exit 2
fi

OUT="$VOICE_DIR/kws_sample_result.txt"

"$KWS_BIN" \
    --print-args=false \
    --tokens="$KWS_TOKENS" \
    --encoder="$KWS_ENCODER" \
    --decoder="$KWS_DECODER" \
    --joiner="$KWS_JOINER" \
    --keywords-file="$KWS_KEYWORDS" \
    --model-type=zipformer2 \
    --num-threads="$KWS_THREADS" \
    --keywords-score="$SCORE" \
    --keywords-threshold="$THRESHOLD" \
    "$WAV" >"$OUT" 2>&1 || true

if grep -q '"keyword"' "$OUT"; then
    echo "KWS_HIT threshold=$THRESHOLD score=$SCORE"
    grep '"keyword"' "$OUT"
    exit 0
fi

echo "KWS_MISS threshold=$THRESHOLD score=$SCORE"
grep -E 'Audio duration|Elapsed|RTF' "$OUT" || true
exit 1
