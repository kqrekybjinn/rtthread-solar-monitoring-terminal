#!/bin/sh
set -eu

WAV="${1:?usage: tencent_asr.sh input.wav}"
CONFIG="${TENCENT_ASR_CONFIG:-/userdata/voice-assistant/tencent_asr.env}"
BIN="${TENCENT_ASR_BIN:-/userdata/voice-assistant/tencent_asr}"
LEVEL_BIN="${WAV_LEVEL_BIN:-/userdata/local-asr/wav_level}"
MIN_PEAK="${ASR_MIN_PEAK:-1000}"
MIN_RMS="${ASR_MIN_RMS:-180}"

if [ -x "$LEVEL_BIN" ]; then
  LEVEL="$("$LEVEL_BIN" "$WAV")"
  PEAK="$(echo "$LEVEL" | sed -n 's/.*peak=\([0-9][0-9]*\).*/\1/p')"
  RMS_INT="$(echo "$LEVEL" | sed -n 's/.*rms=\([0-9][0-9]*\).*/\1/p')"
  PEAK="${PEAK:-0}"
  RMS_INT="${RMS_INT:-0}"
  if [ "$PEAK" -lt "$MIN_PEAK" ] || [ "$RMS_INT" -lt "$MIN_RMS" ]; then
    echo ""
    exit 0
  fi
fi

YEAR="$(date -u +%Y 2>/dev/null || echo 1970)"
if [ "$YEAR" -lt 2024 ] && command -v ntpdate >/dev/null 2>&1; then
  ntpdate -u ntp.tencent.com >/dev/null 2>&1 || \
  ntpdate -u ntp.aliyun.com >/dev/null 2>&1 || true
fi

"$BIN" "$WAV" "$CONFIG"
