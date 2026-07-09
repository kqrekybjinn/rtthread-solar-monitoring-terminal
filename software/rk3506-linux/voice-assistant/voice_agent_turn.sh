#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

SECONDS_ARG="${1:-$RECORD_SECONDS}"

ASR_PROGRAM="${ASR_PROGRAM:-/userdata/voice-assistant/tencent_asr.sh}" \
CHAT_PROGRAM="${CHAT_PROGRAM:-/userdata/voice-assistant/agent_chat.sh}" \
TTS_PROGRAM="${TTS_PROGRAM:-/userdata/voice-assistant/local_tts.sh}" \
"$DIR/ask.sh" "$SECONDS_ARG"
