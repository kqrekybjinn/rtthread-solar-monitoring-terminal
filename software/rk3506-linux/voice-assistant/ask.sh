#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

SECONDS_ARG="${1:-$RECORD_SECONDS}"
IN_WAV="$VOICE_DIR/last_input_16k.wav"
USER_TEXT="$VOICE_DIR/last_user.txt"
REPLY_TEXT="$VOICE_DIR/last_reply.txt"
REPLY_WAV="$VOICE_DIR/last_reply.wav"

mkdir -p "$VOICE_DIR"
echo "recording ${SECONDS_ARG}s..."
"$DIR/record_once.sh" "$SECONDS_ARG" "$IN_WAV" >/dev/null

if [ -z "${ASR_PROGRAM:-}" ] || [ -z "${CHAT_PROGRAM:-}" ] || [ -z "${TTS_PROGRAM:-}" ]; then
    cat >"$VOICE_DIR/last_status.txt" <<EOF
record_ok=1
input_wav=$IN_WAV
reason=cloud_adapters_not_configured
next=configure ASR_PROGRAM, CHAT_PROGRAM, and TTS_PROGRAM in $DIR/config.env
EOF
    echo "录音完成：$IN_WAV"
    echo "还没有配置 ASR/LLM/TTS 适配程序，所以先停在录音阶段。"
    exit 2
fi

"$ASR_PROGRAM" "$IN_WAV" >"$USER_TEXT"

if [ -n "${MQTT_PUBLISH_PROGRAM:-}" ]; then
    "$MQTT_PUBLISH_PROGRAM" "$MQTT_TOPIC_PREFIX/text" "$USER_TEXT" || true
fi

"$CHAT_PROGRAM" "$USER_TEXT" >"$REPLY_TEXT"
"$TTS_PROGRAM" "$REPLY_TEXT" "$REPLY_WAV"

if [ -n "${MQTT_PUBLISH_PROGRAM:-}" ]; then
    "$MQTT_PUBLISH_PROGRAM" "$MQTT_TOPIC_PREFIX/reply" "$REPLY_TEXT" || true
fi

"$DIR/play_wav.sh" "$REPLY_WAV"
echo "done"
