#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
VOICE_DIR="${VOICE_DIR:-$DIR}"
RUNTIME="${SHERPA_TTS_RUNTIME:-$VOICE_DIR/sherpa-onnx}"
MODEL_DIR="${SHERPA_TTS_MODEL_DIR:-$VOICE_DIR/sherpa-tts-model/vits-piper-zh_CN-huayan-x_low}"
MODEL_FILE="${SHERPA_TTS_MODEL_FILE:-$MODEL_DIR/zh_CN-huayan-x_low.onnx}"
LEXICON_FILE="${SHERPA_TTS_LEXICON_FILE:-$MODEL_DIR/lexicon.txt}"
TOKENS_FILE="${SHERPA_TTS_TOKENS_FILE:-$MODEL_DIR/tokens.txt}"
BIN="${SHERPA_TTS_BIN:-$RUNTIME/bin/sherpa-onnx-offline-tts}"
THREADS="${SHERPA_TTS_THREADS:-2}"
SID="${SHERPA_TTS_SID:-1}"
LENGTH_SCALE="${SHERPA_TTS_LENGTH_SCALE:-1.0}"
FALLBACK="${SHERPA_TTS_FALLBACK:-$DIR/local_tts.sh}"

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 text-or-text-file [output.wav]" >&2
    exit 2
fi

INPUT="$1"
OUT="${2:-$VOICE_DIR/reply.wav}"
TMP="/tmp/sherpa_tts_input_$$.txt"
LOG="/tmp/sherpa_tts_$$.log"
trap 'rm -f "$TMP" "$LOG"' EXIT INT TERM

if [ -f "$INPUT" ]; then
    cp "$INPUT" "$TMP"
else
    printf '%s\n' "$INPUT" >"$TMP"
fi

TEXT="$(tr '\r\n' '  ' <"$TMP" | sed 's/[[:space:]][[:space:]]*/ /g; s/^ //; s/ $//')"
if [ -z "$TEXT" ]; then
    TEXT="我没有听清，请再说一遍。"
fi

mkdir -p "$(dirname "$OUT")"

if [ ! -x "$BIN" ] || [ ! -f "$MODEL_FILE" ] || [ ! -f "$LEXICON_FILE" ] || [ ! -f "$TOKENS_FILE" ]; then
    if [ -x "$FALLBACK" ]; then
        "$FALLBACK" "$TMP" "$OUT"
        exit $?
    fi
    echo "sherpa_tts: missing runtime or model" >&2
    exit 1
fi

RULE_FSTS=""
if [ -f "$MODEL_DIR/phone.fst" ] && [ -f "$MODEL_DIR/number.fst" ]; then
    RULE_FSTS="$MODEL_DIR/phone.fst,$MODEL_DIR/number.fst"
elif [ -f "$MODEL_DIR/number.fst" ]; then
    RULE_FSTS="$MODEL_DIR/number.fst"
fi

if [ -n "$RULE_FSTS" ]; then
    "$BIN" \
        --num-threads="$THREADS" \
        --vits-model="$MODEL_FILE" \
        --vits-lexicon="$LEXICON_FILE" \
        --vits-tokens="$TOKENS_FILE" \
        --tts-rule-fsts="$RULE_FSTS" \
        --vits-length-scale="$LENGTH_SCALE" \
        --sid="$SID" \
        --output-filename="$OUT" \
        "$TEXT" >"$LOG" 2>&1 || {
            cat "$LOG" >&2
            if [ -x "$FALLBACK" ]; then
                "$FALLBACK" "$TMP" "$OUT"
                exit $?
            fi
            exit 1
        }
else
    "$BIN" \
        --num-threads="$THREADS" \
        --vits-model="$MODEL_FILE" \
        --vits-lexicon="$LEXICON_FILE" \
        --vits-tokens="$TOKENS_FILE" \
        --vits-length-scale="$LENGTH_SCALE" \
        --sid="$SID" \
        --output-filename="$OUT" \
        "$TEXT" >"$LOG" 2>&1 || {
            cat "$LOG" >&2
            if [ -x "$FALLBACK" ]; then
                "$FALLBACK" "$TMP" "$OUT"
                exit $?
            fi
            exit 1
        }
fi

if [ ! -s "$OUT" ]; then
    echo "sherpa_tts: output wav is empty: $OUT" >&2
    exit 1
fi

printf '%s\n' "$OUT"
