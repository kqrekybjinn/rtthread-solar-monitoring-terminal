#!/bin/sh
set -eu

BASE="${LOCAL_ASR_DIR:-/userdata/local-asr}"
WAV="${1:?usage: local_asr.sh input.wav}"
TMP="${BASE}/last_asr_raw.txt"
MIN_PEAK="${LOCAL_ASR_MIN_PEAK:-1000}"
MIN_RMS="${LOCAL_ASR_MIN_RMS:-180}"

if [ -x "${BASE}/wav_level" ]; then
  LEVEL="$("${BASE}/wav_level" "$WAV")"
  PEAK="$(echo "$LEVEL" | sed -n 's/.*peak=\([0-9][0-9]*\).*/\1/p')"
  RMS_INT="$(echo "$LEVEL" | sed -n 's/.*rms=\([0-9][0-9]*\).*/\1/p')"
  PEAK="${PEAK:-0}"
  RMS_INT="${RMS_INT:-0}"
  if [ "$PEAK" -lt "$MIN_PEAK" ] || [ "$RMS_INT" -lt "$MIN_RMS" ]; then
    echo ""
    exit 0
  fi
fi

"${BASE}/sherpa-onnx-offline" \
  --tokens="${BASE}/model/tokens.txt" \
  --paraformer="${BASE}/model/model.int8.onnx" \
  --model-type=paraformer \
  --num-threads=1 \
  --decoding-method=greedy_search \
  "$WAV" >"$TMP" 2>&1

sed -n 's/.*"text": "\([^"]*\)".*/\1/p' "$TMP" | tail -1
